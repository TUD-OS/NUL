# -*- Mode: Python -*-

e = Environment(CFLAGS=["-g", "-m32", "-O3", "-ffreestanding"],
                LINKFLAGS=["-m32"],
                CPPPATH=[".", "#i386"],
                CC="gcc-4.3.4",
                )

e.Command('softcore.c', ['softwords/softcore.awk',
                         'softwords/softcore.fr',
                         'softwords/jhlocal.fr',
                         'softwords/marker.fr',
                         'softwords/freebsd.fr',
                         'softwords/ficllocal.fr',
                         'softwords/ifbrack.fr',
                         'softwords/oo.fr',
                         'softwords/classes.fr',
                         ],
          """cat ${SOURCES[1:]} | awk -f $SOURCE -v datestamp="`LC_ALL=C date`" > $TARGET""")

e.Program('testmain', ['testmain.c',
                       'dict.c',
                       'ficl.c',
                       'fileaccess.c',
                       'float.c',
                       'loader.c',
                       'math64.c',
                       'prefix.c',
                       'search.c',
                       'stack.c',
                       'softcore.c',
                       'tools.c',
                       'vm.c',
                       'words.c',
                       'i386/sysdep.c',
                       ],
          CPPFLAGS=["-DTESTMAIN", "-D_TESTMAIN",
                    "-D_DEBUG",
                    ])

# EOF
