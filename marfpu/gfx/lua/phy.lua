local phy = {}

local gfx = import "libs/gfx"

ffi.cdef [[
	typedef struct cpContactBufferHeader cpContactBufferHeader;
	typedef struct cpArbiter cpArbiter;
	typedef struct cpShapeClass cpShapeClass;
	typedef struct cpSpaceHashBin cpSpaceHashBin;
	typedef void* cpDataPointer;
	typedef unsigned cpTimestamp;
	typedef unsigned cpCollisionType;
	typedef unsigned cpGroup;
	typedef unsigned cpLayers;
	typedef unsigned cpHashValue;
	typedef int cpBool;

	typedef struct cpArray {
		int num;
		int max;
		void **arr;
	} cpArray;

	typedef double cpFloat;
	typedef struct cpVect { cpFloat x,y; } cpVect;
	typedef struct cpBB { cpFloat l, b, r, t; } cpBB;

	typedef void (*cpBodyVelocityFunc)(struct cpBody *body, cpVect gravity, cpFloat damping, cpFloat dt);
	typedef void (*cpBodyPositionFunc)(struct cpBody *body, cpFloat dt);

	typedef cpBool (*cpCollisionBeginFunc)(cpArbiter *arb, struct cpSpace *space, void *data);
	typedef cpBool (*cpCollisionPreSolveFunc)(cpArbiter *arb, struct cpSpace *space, void *data);
	typedef void (*cpCollisionPostSolveFunc)(cpArbiter *arb, struct cpSpace *space, void *data);
	typedef void (*cpCollisionSeparateFunc)(cpArbiter *arb, struct cpSpace *space, void *data);

	typedef cpBB (*cpSpaceHashBBFunc)(void *obj);

	typedef struct cpComponentNode {
		struct cpBody *parent;
		struct cpBody *next;
		int rank;
		cpFloat idleTime;
	} cpComponentNode;

	typedef struct cpBody {
		
		cpBodyVelocityFunc velocity_func;
		cpBodyPositionFunc position_func;
		cpFloat m, m_inv;
		cpFloat i, i_inv;
		cpVect p, v, f;
		cpFloat a, w, t;
		cpVect rot;
		cpDataPointer data;
		cpFloat v_limit, w_limit;

		cpVect v_bias;
		cpFloat w_bias;
		struct cpSpace *space;
		struct cpShape *shapesList;
		cpComponentNode node;
	} cpBody;

	typedef struct cpCollisionHandler {
		cpCollisionType a;
		cpCollisionType b;
		cpCollisionBeginFunc begin;
		cpCollisionPreSolveFunc preSolve;
		cpCollisionPostSolveFunc postSolve;
		cpCollisionSeparateFunc separate;
		void *data;
	} cpCollisionHandler;

	typedef struct cpHashSetBin {
		void *elt;
		cpHashValue hash;
		struct cpHashSetBin *next;
	} cpHashSetBin;

	typedef cpBool (*cpHashSetEqlFunc)(void *ptr, void *elt);
	typedef void *(*cpHashSetTransFunc)(void *ptr, void *data);

	typedef struct cpHashSet {
		int entries;
		int size;
		cpHashSetEqlFunc eql;
		cpHashSetTransFunc trans;
		void *default_value;
		cpHashSetBin **table;
		cpHashSetBin *pooledBins;
		cpArray *allocatedBuffers;
	} cpHashSet;

	typedef struct cpSpaceHash {
		int numcells;
		cpFloat celldim;
		cpFloat invcelldim;
	
		cpSpaceHashBBFunc bbfunc;

		tl_list shapes;

		cpSpaceHashBin **table;
		cpSpaceHashBin *pooledBins;
	
		cpArray *allocatedBuffers;
		cpTimestamp stamp;
	} cpSpaceHash;

	typedef struct cpSpace {
		int iterations;
		int elasticIterations;
		cpVect gravity;
		cpFloat damping;
		cpFloat idleSpeedThreshold;
		cpFloat sleepTimeThreshold;

		int locked;
		cpTimestamp stamp;
		cpSpaceHash *staticShapes;
		cpSpaceHash *activeShapes;
		cpArray *bodies;
		cpArray *sleepingComponents;
		cpArray *rousedBodies;
		cpArray *arbiters;
		cpArray *pooledArbiters;
		cpContactBufferHeader *contactBuffersHead;
		cpContactBufferHeader *_contactBuffersTail_Deprecated;
		cpArray *allocatedBuffers;
		cpHashSet *contactSet;
		cpArray *constraints;
		cpHashSet *collFuncSet;
		cpCollisionHandler defaultHandler;
		cpHashSet *postStepCallbacks;
	
		cpBody staticBody;
	} cpSpace;

	typedef enum cpShapeType{
		CP_CIRCLE_SHAPE,
		CP_SEGMENT_SHAPE,
		CP_POLY_SHAPE,
		CP_NUM_SHAPES
	} cpShapeType;

	typedef struct cpSegmentQueryInfo cpSegmentQueryInfo;

	typedef struct cpShapeClass {
		cpShapeType type;
		cpBB (*cacheData)(struct cpShape *shape, cpVect p, cpVect rot);
		void (*destroy)(struct cpShape *shape);
		cpBool (*pointQuery)(struct cpShape *shape, cpVect p);
		void (*segmentQuery)(struct cpShape *shape, cpVect a, cpVect b, cpSegmentQueryInfo *info);
	} cpShapeClass;

	typedef struct tl_list_node_cpShape
	{
		struct cpShape* next;
		struct cpShape* prev;
	} tl_list_node_cpShape;

	typedef struct cpSpaceHashNode
	{
		tl_list_node_cpShape node;
		cpTimestamp stamp;
	} cpSpaceHashNode;

	typedef struct cpShape {
		cpSpaceHashNode spaceHashNode;
		const cpShapeClass *klass;

		cpBody *body;
		cpBB bb;
		cpBool sensor;
		cpFloat e;
		cpFloat u;
		cpVect surface_v;
		cpFloat density;
		cpDataPointer data;
		cpCollisionType collision_type;
		cpGroup group;
		cpLayers layers;
	
		struct cpShape *next;
		cpHashValue hashid;
	} cpShape;

	typedef struct cpCircleShape {
		cpShape shape;
	
		cpVect c;
		cpFloat r;
		cpVect tc;
	} cpCircleShape;

	typedef struct cpPolyShapeAxis {
		cpVect n;
		cpFloat d;
	} cpPolyShapeAxis;

	typedef struct cpPolyShape {
		cpShape shape;
		int numVerts;
		cpVect *verts;
		cpPolyShapeAxis *axes;
		cpVect *tVerts;
		cpPolyShapeAxis *tAxes;
	} cpPolyShape;

	void cpInitChipmunk(void);

	cpFloat cpMomentForCircle(cpFloat m, cpFloat r1, cpFloat r2, cpVect offset);

	cpSpace* cpSpaceInit(cpSpace *space);
	void     cpSpaceDestroy(cpSpace *space);
	cpBody*  cpSpaceAddBody(cpSpace *space, cpBody *body);
	cpShape* cpSpaceAddStaticShape(cpSpace *space, cpShape *shape);
	cpShape* cpSpaceAddShape(cpSpace *space, cpShape *shape);
	void     cpSpaceStep(cpSpace *space, cpFloat dt);

	void cpSpaceResizeActiveHash(cpSpace* space, cpFloat dim, int count);

	cpBody*  cpBodyInit(cpBody *body, cpFloat m, cpFloat i);
	void     cpBodyUpdateMassMoment(cpBody *body);
	

	cpCircleShape *cpCircleShapeInit(cpCircleShape *circle, cpBody *body, cpFloat radius, cpVect offset);
	cpPolyShape *cpBoxShapeInit(cpPolyShape *poly, cpBody *body, cpFloat width, cpFloat height);
]]


local p = gfx.g
local C = ffi.C
local cpVect = ffi.typeof "cpVect"
phy.vect = cpVect
local cpCircleShape = ffi.typeof "cpCircleShape"
local cpPolyShape = ffi.typeof "cpPolyShape"
phy.circle = cpCircleShape
phy.polygon = cpPolyShape

phy.CP_CIRCLE_SHAPE = 0
phy.CP_SEGMENT_SHAPE = 1
phy.CP_POLY_SHAPE = 2
phy.CP_NUM_SHAPES = 3

p.cpInitChipmunk()

-- TODO: p.cpSpaceDestroy(space)

function phy.create_space(gravity)
	local space = ffi.new("cpSpace")
	p.cpSpaceInit(space)
	p.cpSpaceResizeActiveHash(space, 3, 40000)
	space.iterations = 10
	space.gravity = gravity or cpVect(0, 100)
	return {space, bodies = {}, shapes = {}}
end

function phy.create_body(space, mass)
	local body = ffi.new("cpBody")
	p.cpBodyInit(body, mass, 1)
	space.bodies[#space.bodies + 1] = body -- To keep it alive
	p.cpSpaceAddBody(space[1], body)
	return body
end

function phy.static_body(space)
	return space[1].staticBody
end

function phy.new_circle(space, body, radius, pos)
	local shape = cpCircleShape()
	p.cpCircleShapeInit(shape, body, radius, pos or cpVect(0, 0))
	space.shapes[#space.shapes + 1] = shape -- To keep it alive
	p.cpSpaceAddShape(space[1], shape.shape)
	return shape
end

function phy.new_box(space, body, width, height)
	local shape = cpPolyShape()
	p.cpBoxShapeInit(shape, body, width, height);
	space.shapes[#space.shapes + 1] = shape -- To keep it alive
	p.cpSpaceAddShape(space[1], shape.shape)
	return shape
end

function phy.balance_body(body)
	p.cpBodyUpdateMassMoment(body)
end

function phy.step(space, dt)
	p.cpSpaceStep(space[1], dt)
end

function phy.is_sleeping(body)
	return body.node.next ~= nil
end

function phy.each_body(space, f)
	
	local bodies = space[1].bodies

	for i = 0, bodies.num-1 do
		f(ffi.cast("cpBody*", bodies.arr[i]))
	end

	local components = space[1].sleepingComponents

	for i = 0, components.num-1 do
		local root = ffi.cast("cpBody*", components.arr[i])
		local body = root
		repeat
			local next = body.node.next
			f(body)
			body = next
		until body ~= root
	end
end

local node_offset = ffi.offsetof("cpShape", "spaceHashNode") + ffi.offsetof("cpSpaceHashNode", "node")
assert(node_offset == 0)

local function list_each(list, f)
	local v = ffi.cast("cpShape*", list.sentinel.next)
	local sentinel = ffi.cast("cpShape*", list)

	while v ~= sentinel do
		f(v)
		v = v.spaceHashNode.node.next
	end
end

function phy.each_shape_in_space(space, f)
	local sp = space[1]
	list_each(sp.activeShapes.shapes, f)
	list_each(sp.staticShapes.shapes, f)
end

function phy.each_shape(body, f)
	local shape = body.shapesList

	while shape ~= nil do
		f(shape)
		shape = shape.next
	end
end

return phy