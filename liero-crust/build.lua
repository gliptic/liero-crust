local gen_gfx = require 'gfx/_build/gen'

local path = ...

gen_gfx.run(path, path .. 'gfx/GL/', path .. 'gfx/glext.txt')