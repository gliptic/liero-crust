//#include "avcodec.h"
//#include "get_bits.h"
//#include "put_bits.h"
//#include "dsputil.h"
//#include "thread.h"

#include <tl/platform.h>

#define VLC_BITS 11

#if TL_BIG_ENDIAN
#define B 3
#define G 2
#define R 1
#define A 0
#else
#define B 0
#define G 1
#define R 2
#define A 3
#endif

typedef enum Predictor{
    LEFT= 0,
    PLANE,
    MEDIAN,
} Predictor;

typedef struct HYuvContext{
    AVCodecContext *avctx;
    Predictor predictor;
    GetBitContext gb;
    PutBitContext pb;
    int interlaced;
    int decorrelate;
    int bitstream_bpp;
    int version;
    int yuy2;                               //use yuy2 instead of 422P
    int bgr32;                              //use bgr32 instead of bgr24
    int width, height;
    int flags;
    int context;
    int picture_number;
    int last_slice_end;
    uint8_t *temp[3];
    uint64_t stats[3][256];
    uint8_t len[3][256];
    uint32_t bits[3][256];
    uint32_t pix_bgr_map[1<<VLC_BITS];
    VLC vlc[6];                             //Y,U,V,YY,YU,YV
    AVFrame picture;
    uint8_t *bitstream_buffer;
    unsigned int bitstream_buffer_size;
    DSPContext dsp;
}HYuvContext;

static inline int sub_left_prediction(HYuvContext *s, uint8_t *dst, uint8_t *src, int w, int left){
    int i;
    if(w<32){
        for(i=0; i<w; i++){
            const int temp= src[i];
            dst[i]= temp - left;
            left= temp;
        }
        return left;
    }else{
        for(i=0; i<16; i++){
            const int temp= src[i];
            dst[i]= temp - left;
            left= temp;
        }
        s->dsp.diff_bytes(dst+16, src+16, src+15, w-16);
        return src[w-1];
    }
}

static int read_len_table(uint8_t *dst, GetBitContext *gb){
    int i, val, repeat;

    for(i=0; i<256;){
        repeat= get_bits(gb, 3);
        val   = get_bits(gb, 5);
        if(repeat==0)
            repeat= get_bits(gb, 8);
//printf("%d %d\n", val, repeat);
        if(i+repeat > 256) {
            av_log(NULL, AV_LOG_ERROR, "Error reading huffman table\n");
            return -1;
        }
        while (repeat--)
            dst[i++] = val;
    }
    return 0;
}

static int generate_bits_table(uint32_t *dst, const uint8_t *len_table){
    int len, index;
    uint32_t bits=0;

    for(len=32; len>0; len--){
        for(index=0; index<256; index++){
            if(len_table[index]==len)
                dst[index]= bits++;
        }
        if(bits & 1){
            av_log(NULL, AV_LOG_ERROR, "Error generating huffman table\n");
            return -1;
        }
        bits >>= 1;
    }
    return 0;
}

#if CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER
typedef struct {
    uint64_t val;
    int name;
} HeapElem;

static void heap_sift(HeapElem *h, int root, int size)
{
    while(root*2+1 < size) {
        int child = root*2+1;
        if(child < size-1 && h[child].val > h[child+1].val)
            child++;
        if(h[root].val > h[child].val) {
            FFSWAP(HeapElem, h[root], h[child]);
            root = child;
        } else
            break;
    }
}

static void generate_len_table(uint8_t *dst, const uint64_t *stats){
    HeapElem h[256];
    int up[2*256];
    int len[2*256];
    int offset, i, next;
    int size = 256;

    for(offset=1; ; offset<<=1){
        for(i=0; i<size; i++){
            h[i].name = i;
            h[i].val = (stats[i] << 8) + offset;
        }
        for(i=size/2-1; i>=0; i--)
            heap_sift(h, i, size);

        for(next=size; next<size*2-1; next++){
            // merge the two smallest entries, and put it back in the heap
            uint64_t min1v = h[0].val;
            up[h[0].name] = next;
            h[0].val = INT64_MAX;
            heap_sift(h, 0, size);
            up[h[0].name] = next;
            h[0].name = next;
            h[0].val += min1v;
            heap_sift(h, 0, size);
        }

        len[2*size-2] = 0;
        for(i=2*size-3; i>=size; i--)
            len[i] = len[up[i]] + 1;
        for(i=0; i<size; i++) {
            dst[i] = len[up[i]] + 1;
            if(dst[i] >= 32) break;
        }
        if(i==size) break;
    }
}
#endif /* CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER */

static void generate_joint_tables(HYuvContext *s){
    uint16_t symbols[1<<VLC_BITS];
    uint16_t bits[1<<VLC_BITS];
    uint8_t len[1<<VLC_BITS];
    if(s->bitstream_bpp < 24){
        int p, i, y, u;
        for(p=0; p<3; p++){
            for(i=y=0; y<256; y++){
                int len0 = s->len[0][y];
                int limit = VLC_BITS - len0;
                if(limit <= 0)
                    continue;
                for(u=0; u<256; u++){
                    int len1 = s->len[p][u];
                    if(len1 > limit)
                        continue;
                    len[i] = len0 + len1;
                    bits[i] = (s->bits[0][y] << len1) + s->bits[p][u];
                    symbols[i] = (y<<8) + u;
                    if(symbols[i] != 0xffff) // reserved to mean "invalid"
                        i++;
                }
            }
            free_vlc(&s->vlc[3+p]);
            init_vlc_sparse(&s->vlc[3+p], VLC_BITS, i, len, 1, 1, bits, 2, 2, symbols, 2, 2, 0);
        }
    }else{
        
    }
}

static int read_huffman_tables(HYuvContext *s, const uint8_t *src, int length){
    GetBitContext gb;
    int i;

    init_get_bits(&gb, src, length*8);

    for(i=0; i<3; i++){
        if(read_len_table(s->len[i], &gb)<0)
            return -1;
        if(generate_bits_table(s->bits[i], s->len[i])<0){
            return -1;
        }

        free_vlc(&s->vlc[i]);
        init_vlc(&s->vlc[i], VLC_BITS, 256, s->len[i], 1, 1, s->bits[i], 4, 4, 0);
    }

    generate_joint_tables(s);

    return (get_bits_count(&gb)+7)/8;
}

static av_cold void alloc_temp(HYuvContext *s){
    int i;

    if(s->bitstream_bpp<24){
        for(i=0; i<3; i++){
            s->temp[i]= av_malloc(s->width + 16);
        }
    }else{
        s->temp[0]= av_mallocz(4*s->width + 16);
    }
}

static av_cold int common_init(AVCodecContext *avctx){
    HYuvContext *s = avctx->priv_data;

    s->avctx= avctx;
    s->flags= avctx->flags;

    dsputil_init(&s->dsp, avctx);

    s->width= avctx->width;
    s->height= avctx->height;
    assert(s->width>0 && s->height>0);

    return 0;
}

#if CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER
static int store_table(HYuvContext *s, const uint8_t *len, uint8_t *buf){
    int i;
    int index= 0;

    for(i=0; i<256;){
        int val= len[i];
        int repeat=0;

        for(; i<256 && len[i]==val && repeat<255; i++)
            repeat++;

        assert(val < 32 && val >0 && repeat<256 && repeat>0);
        if(repeat>7){
            buf[index++]= val;
            buf[index++]= repeat;
        }else{
            buf[index++]= val | (repeat<<5);
        }
    }

    return index;
}

static av_cold int encode_init(AVCodecContext *avctx)
{
    HYuvContext *s = avctx->priv_data;
    int i, j;

    common_init(avctx);

    avctx->extradata= av_mallocz(1024*30); // 256*3+4 == 772
    avctx->stats_out= av_mallocz(1024*30); // 21*256*3(%llu ) + 3(\n) + 1(0) = 16132
    s->version=2;

    avctx->coded_frame= &s->picture;

    switch(avctx->pix_fmt){
    case PIX_FMT_YUV420P:
        s->bitstream_bpp= 12;
        break;
    case PIX_FMT_YUV422P:
        s->bitstream_bpp= 16;
        break;
    case PIX_FMT_RGB32:
        s->bitstream_bpp= 24;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "format not supported\n");
        return -1;
    }
    avctx->bits_per_coded_sample= s->bitstream_bpp;
    s->decorrelate= s->bitstream_bpp >= 24;
    s->predictor= avctx->prediction_method;
    s->interlaced= avctx->flags&CODEC_FLAG_INTERLACED_ME ? 1 : 0;
    if(avctx->context_model==1){
        s->context= avctx->context_model;
        if(s->flags & (CODEC_FLAG_PASS1|CODEC_FLAG_PASS2)){
            av_log(avctx, AV_LOG_ERROR, "context=1 is not compatible with 2 pass huffyuv encoding\n");
            return -1;
        }
    }else s->context= 0;

    if(avctx->codec->id==CODEC_ID_HUFFYUV){
        if(avctx->pix_fmt==PIX_FMT_YUV420P){
            av_log(avctx, AV_LOG_ERROR, "Error: YV12 is not supported by huffyuv; use vcodec=ffvhuff or format=422p\n");
            return -1;
        }
        if(avctx->context_model){
            av_log(avctx, AV_LOG_ERROR, "Error: per-frame huffman tables are not supported by huffyuv; use vcodec=ffvhuff\n");
            return -1;
        }
        if(s->interlaced != ( s->height > 288 ))
            av_log(avctx, AV_LOG_INFO, "using huffyuv 2.2.0 or newer interlacing flag\n");
    }

    if(s->bitstream_bpp>=24 && s->predictor==MEDIAN){
        av_log(avctx, AV_LOG_ERROR, "Error: RGB is incompatible with median predictor\n");
        return -1;
    }

    ((uint8_t*)avctx->extradata)[0]= s->predictor | (s->decorrelate << 6);
    ((uint8_t*)avctx->extradata)[1]= s->bitstream_bpp;
    ((uint8_t*)avctx->extradata)[2]= s->interlaced ? 0x10 : 0x20;
    if(s->context)
        ((uint8_t*)avctx->extradata)[2]|= 0x40;
    ((uint8_t*)avctx->extradata)[3]= 0;
    s->avctx->extradata_size= 4;

    if(avctx->stats_in){
	/*
        char *p= avctx->stats_in;

        for(i=0; i<3; i++)
            for(j=0; j<256; j++)
                s->stats[i][j]= 1;

        for(;;){
            for(i=0; i<3; i++){
                char *next;

                for(j=0; j<256; j++){
                    s->stats[i][j]+= strtol(p, &next, 0);
                    if(next==p) return -1;
                    p=next;
                }
            }
            if(p[0]==0 || p[1]==0 || p[2]==0) break;
        }*/
    }else{
        for(i=0; i<3; i++)
            for(j=0; j<256; j++){
                int d= FFMIN(j, 256-j);

                s->stats[i][j]= 100000000/(d+1);
            }
    }

    for(i=0; i<3; i++){
        generate_len_table(s->len[i], s->stats[i]);

        if(generate_bits_table(s->bits[i], s->len[i])<0){
            return -1;
        }

        s->avctx->extradata_size+=
        store_table(s, s->len[i], &((uint8_t*)s->avctx->extradata)[s->avctx->extradata_size]);
    }

    if(s->context){
        for(i=0; i<3; i++){
            int pels = s->width*s->height / (i?40:10);
            for(j=0; j<256; j++){
                int d= FFMIN(j, 256-j);
                s->stats[i][j]= pels/(d+1);
            }
        }
    }else{
        for(i=0; i<3; i++)
            for(j=0; j<256; j++)
                s->stats[i][j]= 0;
    }

//    printf("pred:%d bpp:%d hbpp:%d il:%d\n", s->predictor, s->bitstream_bpp, avctx->bits_per_coded_sample, s->interlaced);

    alloc_temp(s);

    s->picture_number=0;

    return 0;
}
#endif /* CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER */

#if CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER
static int encode_422_bitstream(HYuvContext *s, int offset, int count){
    int i;
    const uint8_t *y = s->temp[0] + offset;
    const uint8_t *u = s->temp[1] + offset/2;
    const uint8_t *v = s->temp[2] + offset/2;

    if(s->pb.buf_end - s->pb.buf - (put_bits_count(&s->pb)>>3) < 2*4*count){
        av_log(s->avctx, AV_LOG_ERROR, "encoded frame too large\n");
        return -1;
    }

#define LOAD4\
            int y0 = y[2*i];\
            int y1 = y[2*i+1];\
            int u0 = u[i];\
            int v0 = v[i];

    count/=2;
    if(s->flags&CODEC_FLAG_PASS1){
        for(i=0; i<count; i++){
            LOAD4;
            s->stats[0][y0]++;
            s->stats[1][u0]++;
            s->stats[0][y1]++;
            s->stats[2][v0]++;
        }
    }
    if(s->avctx->flags2&CODEC_FLAG2_NO_OUTPUT)
        return 0;
    if(s->context){
        for(i=0; i<count; i++){
            LOAD4;
            s->stats[0][y0]++;
            put_bits(&s->pb, s->len[0][y0], s->bits[0][y0]);
            s->stats[1][u0]++;
            put_bits(&s->pb, s->len[1][u0], s->bits[1][u0]);
            s->stats[0][y1]++;
            put_bits(&s->pb, s->len[0][y1], s->bits[0][y1]);
            s->stats[2][v0]++;
            put_bits(&s->pb, s->len[2][v0], s->bits[2][v0]);
        }
    }else{
        for(i=0; i<count; i++){
            LOAD4;
            put_bits(&s->pb, s->len[0][y0], s->bits[0][y0]);
            put_bits(&s->pb, s->len[1][u0], s->bits[1][u0]);
            put_bits(&s->pb, s->len[0][y1], s->bits[0][y1]);
            put_bits(&s->pb, s->len[2][v0], s->bits[2][v0]);
        }
    }
    return 0;
}

static int common_end(HYuvContext *s){
    int i;

    for(i=0; i<3; i++){
        av_freep(&s->temp[i]);
    }
    return 0;
}

#if CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER
static int encode_frame(AVCodecContext *avctx, unsigned char *buf, int buf_size, void *data){
    HYuvContext *s = avctx->priv_data;
    AVFrame *pict = data;
    const int width= s->width;
    const int width2= s->width>>1;
    const int height= s->height;
    const int fake_ystride= s->interlaced ? pict->linesize[0]*2  : pict->linesize[0];
    const int fake_ustride= s->interlaced ? pict->linesize[1]*2  : pict->linesize[1];
    const int fake_vstride= s->interlaced ? pict->linesize[2]*2  : pict->linesize[2];
    AVFrame * const p= &s->picture;
    int i, j, size=0;

    *p = *pict;
    p->pict_type= FF_I_TYPE;
    p->key_frame= 1;

    if(s->context){
        for(i=0; i<3; i++){
            generate_len_table(s->len[i], s->stats[i]);
            if(generate_bits_table(s->bits[i], s->len[i])<0)
                return -1;
            size+= store_table(s, s->len[i], &buf[size]);
        }

        for(i=0; i<3; i++)
            for(j=0; j<256; j++)
                s->stats[i][j] >>= 1;
    }

    init_put_bits(&s->pb, buf+size, buf_size-size);

    if(avctx->pix_fmt == PIX_FMT_YUV422P || avctx->pix_fmt == PIX_FMT_YUV420P){
        int lefty, leftu, leftv, y, cy;

        put_bits(&s->pb, 8, leftv= p->data[2][0]);
        put_bits(&s->pb, 8, lefty= p->data[0][1]);
        put_bits(&s->pb, 8, leftu= p->data[1][0]);
        put_bits(&s->pb, 8,        p->data[0][0]);

        lefty= sub_left_prediction(s, s->temp[0], p->data[0], width , 0);
        leftu= sub_left_prediction(s, s->temp[1], p->data[1], width2, 0);
        leftv= sub_left_prediction(s, s->temp[2], p->data[2], width2, 0);

        encode_422_bitstream(s, 2, width-2);

        if(s->predictor==MEDIAN){
            
        }else{
            for(cy=y=1; y<height; y++,cy++){
                uint8_t *ydst, *udst, *vdst;

                /* encode a luma only line & y++ */
                if(s->bitstream_bpp==12){
                    ydst= p->data[0] + p->linesize[0]*y;

                    if(s->predictor == PLANE && s->interlaced < y){
                        s->dsp.diff_bytes(s->temp[1], ydst, ydst - fake_ystride, width);

                        lefty= sub_left_prediction(s, s->temp[0], s->temp[1], width , lefty);
                    }else{
                        lefty= sub_left_prediction(s, s->temp[0], ydst, width , lefty);
                    }
                    encode_gray_bitstream(s, width);
                    y++;
                    if(y>=height) break;
                }

                ydst= p->data[0] + p->linesize[0]*y;
                udst= p->data[1] + p->linesize[1]*cy;
                vdst= p->data[2] + p->linesize[2]*cy;

                if(s->predictor == PLANE && s->interlaced < cy){
                    
                }else{
                    lefty= sub_left_prediction(s, s->temp[0], ydst, width , lefty);
                    leftu= sub_left_prediction(s, s->temp[1], udst, width2, leftu);
                    leftv= sub_left_prediction(s, s->temp[2], vdst, width2, leftv);
                }

                encode_422_bitstream(s, 0, width);
            }
        }
    }else if(avctx->pix_fmt == PIX_FMT_RGB32){
        
    }else{
        av_log(avctx, AV_LOG_ERROR, "Format not supported!\n");
    }
    emms_c();

    size+= (put_bits_count(&s->pb)+31)/8;
    put_bits(&s->pb, 16, 0);
    put_bits(&s->pb, 15, 0);
    size/= 4;

    if((s->flags&CODEC_FLAG_PASS1) && (s->picture_number&31)==0){
        int j;
        char *p= avctx->stats_out;
        char *end= p + 1024*30;
        for(i=0; i<3; i++){
            for(j=0; j<256; j++){
                snprintf(p, end-p, "%"PRIu64" ", s->stats[i][j]);
                p+= strlen(p);
                s->stats[i][j]= 0;
            }
            snprintf(p, end-p, "\n");
            p++;
        }
    } else
        avctx->stats_out[0] = '\0';
    if(!(s->avctx->flags2 & CODEC_FLAG2_NO_OUTPUT)){
        flush_put_bits(&s->pb);
        s->dsp.bswap_buf((uint32_t*)buf, (uint32_t*)buf, size);
    }

    s->picture_number++;

    return size*4;
}

static av_cold int encode_end(AVCodecContext *avctx)
{
    HYuvContext *s = avctx->priv_data;

    common_end(s);

    av_freep(&avctx->extradata);
    av_freep(&avctx->stats_out);

    return 0;
}
#endif /* CONFIG_HUFFYUV_ENCODER || CONFIG_FFVHUFF_ENCODER */
