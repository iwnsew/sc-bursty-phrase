VERSION= '0.0.1'
APPNAME= 'burstyngram'

srcdir= '.'
blddir= 'bin'

def set_options(ctx):
  ctx.tool_options('compiler_cxx')
    
def configure(ctx):
  ctx.check_tool('compiler_cxx')
  ctx.env.CXXFLAGS += ['-O2', '-Wall', '-g', '-std=c++11']

def build(bld):
  task1= bld(features='cxx cprogram',
       source       = 'burstyphrase.cpp',
       name         = 'burstyphrase',
       target       = 'burstyphrase',
       includes     = '.')
