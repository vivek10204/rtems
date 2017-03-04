
rtems_tests_PROGRAMS = capture
capture_SOURCES = init.c test1.c system.h

dist_rtems_tests_DATA = capture.scn
dist_rtems_tests_DATA += capture.doc

include $(RTEMS_ROOT)/make/custom/@RTEMS_BSP@.cfg
include $(top_srcdir)/../automake/compile.am
include $(top_srcdir)/../automake/leaf.am

AM_CPPFLAGS += -I/home/vivek/sandbox/rtems-libbsd/rtemsbsd/include
AM_CPPFLAGS += -I/home/vivek/sandbox/rtems-libbsd/freebsd/include
AM_CPPFLAGS += -I/home/vivek/sandbox/rtems-libbsd/freebsd/sys

LINK_OBJS = $(capture_OBJECTS)
LINK_LIBS = $(capture_LDLIBS)
LINK_LIBS += /home/vivek/sandbox/rtems-libbsd/build/arm-rtems4.12-xilinx_zynq_a9_qemu/libbsd.a -lm

capture$(EXEEXT): $(capture_OBJECTS) $(capture_DEPENDENCIES)
	@rm -f capture$(EXEEXT)
	$(make-exe)

include $(top_srcdir)/../automake/local.am
