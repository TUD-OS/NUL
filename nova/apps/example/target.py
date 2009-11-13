env = _env_.copy()
env["CFLAGS"] =  "-m32 -MD -Os -ffunction-sections -fstrict-aliasing -fno-exceptions -fshort-enums --param max-inline-insns-single=100 -mregparm=3 -fomit-frame-pointer -minline-all-stringops  -g -nostdinc -Waggregate-return -Wattributes -Wcast-align -Wdeprecated-declarations -Wextra -Wmissing-noreturn -Wpacked -Wshadow -Wstack-protector -Wstrict-aliasing -Wswitch -Wswitch-default -Wswitch-enum -Wsystem-headers -Wunsafe-loop-optimizations -Wvolatile-register-var -Wdisabled-optimization -Wformat -Wreturn-type -Wuninitialized -Wunused"

env["include"] = ["../", "../../include/", "../../include/libc", "/usr/lib/gcc/i486-linux-gnu/4.3.4/include"] 
env["linkfile"]= "roottask.ld"
App("example.nova",
    {"src"         :  ["start.S", "main.c"],
     }, 
    env=env)
