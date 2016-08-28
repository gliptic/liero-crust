local gen_model = require 'data/model'

local path = ...

gen_model.run(path, path .. 'data/model.hpp', path .. 'data/model.cpp')