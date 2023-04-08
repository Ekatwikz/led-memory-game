# Stripped version of my phat makefile: https://github.com/Ekatwikz/katwikOpsys/blob/dev/testing/makefile

DEFAULTFILES:=led-memory-game

# objects and paths to use
OWNLIBS:=errorHelpers easyCheck
OWNLIBPATH=.
OWNLIBNAME=katwikOpsys
OWNOBJFOLDER=obj
OWNSRCFOLDER=src

AUTOCLEANOWNOBJS:=0
HDRPATH=$(OWNLIBPATH)/$(OWNLIBNAME)
OBJPATH=$(HDRPATH)/$(OWNOBJFOLDER)
SRCPATH=$(HDRPATH)/$(OWNSRCFOLDER)
OWNOBJS=$(OWNLIBS:%=$(OBJPATH)/%.o)

# TODO: REMOVE PTHREADS!!!!
LIBS:=gpiod pthread
WARNINGS:=all extra pedantic unused-macros
MYFFLAGS:=track-macro-expansion=0 no-omit-frame-pointer

# boonk stuff stuff for me headers
PREDEFINES+=-D '_GNU_SOURCE=1'
PREDEFINES+=-D '_POSIX_C_SOURCE=200809L'
PREDEFINES+=-D 'ERR_MULTIPROCESS=0'
PREDEFINES+=-D 'MUNDANE_MESSAGES=0'
# Can't use addr2line on tiny system, + the way I used popen() bugs out on it anyway idk
PREDEFINES+=-D 'EXEC_ALLOWED=0'

COMPILEFLAGS:=$(CFLAGS) $(HDRPATH:%=-I%) $(WARNINGS:%=-W%) $(MYFFLAGS:%=-f%) $(PREDEFINES) $(CUSTOMINCLUDE:%=-I%) $(DEBUGFLAGS)
LINKFLAGS:=$(LIBS:%=-l%) $(CUSTOMLD:%=-L%)

.PHONY: clean all

all: $(DEFAULTFILES)

$(OBJPATH)/%.o: $(SRCPATH)/%.c $(HDRPATH)/%.h
	mkdir -pv $(OBJPATH)
	$(CC) $(COMPILEFLAGS) -c $< -o $@ $(LINKFLAGS)

$(DEFAULTFILES): $(DEFAULTFILES:%=%.c) $(OWNOBJS)
	$(CC) $(COMPILEFLAGS) $(OWNOBJS) $< -o $@ $(LINKFLAGS)

clean:
	rm -fv $(DEFAULTFILES)
	rm -fv $(OBJPATH)/*.o

