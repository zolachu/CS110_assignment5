# CS110 Makefile Hooks: aggregate

PROGS = aggregate
EXTRA_PROGS = tptest tpcustomtest test-union-and-intersection
CXX = /usr/bin/g++-5

NA_LIB_SRC = news-aggregator.cc \
	     log.cc \
	     utils.cc \
	     rss-index.cc

TP_LIB_SRC = thread-pool.cc

WARNINGS = -Wall -pedantic
DEPS = -MMD -MF $(@:.o=.d)
DEFINES = -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD
INCLUDES = -I/afs/ir/class/cs110/local/include -I/usr/include/libxml2 -isystem /usr/class/cs110/include/netlib -I/usr/class/cs110/include/myhtml

CXXFLAGS = -g $(WARNINGS) -O0 -std=c++14 $(DEPS) $(DEFINES) $(INCLUDES)
LDFLAGS = -lm -lxml2 -L/afs/ir/class/cs110/local/lib -lrand -lthreadpoolrelease -lthreads -lrssnet -pthread \
          -L/usr/class/cs110/lib/netlib -lcppnetlib-client-connections -lcppnetlib-uri -lcppnetlib-server-parsers \
          -L/usr/class/cs110/lib/myhtml -lmyhtml \
          -lssl -lcrypto -ldl

NA_LIB_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(NA_LIB_SRC)))
NA_LIB_DEP = $(patsubst %.o,%.d,$(NA_LIB_OBJ))
NA_LIB = libna.a

TP_LIB_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(TP_LIB_SRC)))
TP_LIB_DEP = $(patsubst %.o,%.d,$(TP_LIB_OBJ))
TP_LIB = libthreadpool.a

PROGS_SRC = aggregate.cc
PROGS_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(PROGS_SRC)))
PROGS_DEP = $(patsubst %.o,%.d,$(PROGS_OBJ))

EXTRA_PROGS_SRC = tptest.cc tpcustomtest.cc test-union-and-intersection.cc
EXTRA_PROGS_OBJ = $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(EXTRA_PROGS_SRC)))
EXTRA_PROGS_DEP = $(patsubst %.o,%.d,$(EXTRA_PROGS_OBJ))

all: $(NA_LIB) $(TP_LIB) $(PROGS) $(EXTRA_PROGS)

$(PROGS): %:%.o $(NA_LIB) $(TP_LIB)
	$(CXX) $^ $(LDFLAGS) -o $@

$(EXTRA_PROGS): %:%.o $(TP_LIB)
	$(CXX) $^ $(LDFLAGS) -o $@

$(NA_LIB): $(NA_LIB_OBJ)
	rm -f $@
	ar r $@ $^
	ranlib $@

$(TP_LIB): $(TP_LIB_OBJ)
	rm -f $@
	ar r $@ $^
	ranlib $@

clean:
	@rm -f $(PROGS) $(EXTRA_PROGS) $(PROGS_OBJ) $(EXTRA_PROGS_OBJ) $(PROGS_DEP) $(EXTRA_PROGS_DEP)
	@rm -f $(NA_LIB) $(NA_LIB_DEP) $(NA_LIB_OBJ)
	@rm -f $(TP_LIB) $(TP_LIB_DEP) $(TP_LIB_OBJ)
	@rm -f tpadvtest tpadvtest.*

spartan: clean
	@\rm -fr *~

.PHONY: all clean spartan

-include $(NA_LIB_DEP) $(TP_LIB_DEP) $(PROGS_DEP) $(EXTRA_PROGS_DEP)
