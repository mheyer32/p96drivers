#
#	$Id: GNUmakefile 1.1 1997/05/08 23:50:30 Alex Exp Alex $
#

FILES :=	$(wildcard *.c *.h *.asm *.i *.rev *.fd *.doc smakefile GNUmakefile SCOPTIONS)
DIRS :=

###############################################################################
#
#	Ab hier bitte nichts mehr manuell �ndern
#
###############################################################################

all:
	@echo "Dieses GNUmakefile bitte nicht direkt aufrufen."

checkin:
	@-makedir >nil: RCS
	$(foreach file,$(FILES),ci -M -l -m. -t-. $(file); )
ifneq ($(DIRS),)
	$(foreach dir,$(DIRS),cd "$(dir)"; $(MAKE) -f GNUmakefile $(TESTING) $@ MAKE=$(MAKE); cd /;)
endif

freeze:
	$(foreach file,$(FILES),rcs -q -n$(SYMBOLIC_NAME):  $(file); )
ifneq ($(DIRS),)
	$(foreach dir,$(DIRS),cd "$(dir)"; \
		$(MAKE) -f GNUmakefile $(TESTING) $@ SYMBOLIC_NAME=$(SYMBOLIC_NAME) MAKE=$(MAKE); \
		cd /;)
endif

.PHONY: all checkin freeze
.SUFFIXES: .c .h .asm .i .rev .doc .fd
