env = _env_.copy()
env["CXXFLAGS"] =  "-m32 -MD -Os -ffunction-sections -fstrict-aliasing -fno-rtti -fno-exceptions -fcheck-new -fshort-enums --param max-inline-insns-single=100 -mregparm=3 -fomit-frame-pointer -minline-all-stringops  -g -nostdinc  -Wctor-dtor-privacy -Wdeprecated -Winvalid-offsetof -Wnon-template-friend -Wold-style-cast -Woverloaded-virtual -Wpmf-conversions -Wreorder -Wsign-promo -Wstrict-null-sentinel -Wsynth -Waggregate-return -Wattributes -Wcast-align -Wdeprecated-declarations -Wextra -Wmissing-noreturn -Wpacked -Wshadow -Wstack-protector -Wstrict-aliasing -Wswitch -Wswitch-default -Wswitch-enum -Wsystem-headers -Wunsafe-loop-optimizations -Wvolatile-register-var -Wdisabled-optimization -Wformat -Wreturn-type -Wno-non-virtual-dtor -Wuninitialized -Wunused"

env["include"] = ["../", "../../include/", "../../include/libc", "/usr/lib/gcc/i486-linux-gnu/4.3.4/include"] 
env["linkfile"]= "linker.ld"
App("gt.nova",
    {"src"         :  ["gt.cc", "nova/sys/asm.S", "nova/sys/vprintf.cc", "nova/sys/logging.cc", "nova/sys/simplemalloc.cc"],
     "deps"        :  ["nova/sys/libnovasys"]
     }, 
    env=env)
