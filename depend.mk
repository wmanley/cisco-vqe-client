###############################################################################
# depend.mk
#
# Copyright (c) 2006-2007 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################
#=============================================
# Depend
#
ifeq ($(DEP_FILES),)
DEP_FILES = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(subst .s,.dep, $(subst .S,.dep, $(subst .c,.dep, $(subst .cpp,.dep, $(sort $(SRC)))))))
endif

.PHONY: depend

depend:
	$(RM) $(DEP_FILES)
	@$(MAKE) NO_DEPEND=true $(DEP_FILES)

$(MODOBJ)/%.dep : $(SRCDIR)/%.c
	@mkdir -p $(MODOBJ)
	$(CC) -M $(CFLAGS) $(INCLUDES) $< > $@.$$$$;\
		sed 's,\($*\)\.o[ :]*,$(MODOBJ)/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

$(MODOBJ)/%.dep : $(SRCDIR)/%.S
	@mkdir -p $(MODOBJ)
	$(CC) -M $(CFLAGS) $< > $@.$$$$;\
		sed 's,\($*\)\.o[ :]*,$(MODOBJ)/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$


$(MODOBJ)/%.dep : $(SRCDIR)/%.s
	@mkdir -p $(MODOBJ)
	$(CC) -M $(CFLAGS) $< > $@.$$$$;\
		sed 's,\($*\)\.o[ :]*,$(MODOBJ)/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

$(MODOBJ)/%.dep : $(SRCDIR)/%.cpp
	@mkdir -p $(MODOBJ)
	$(CXX) -M $(CXXFLAGS) -o $(*F).opp $< > $@.$$$$;\
		sed 's,\($*\)\.o[ :]*,$(MODOBJ)/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

$(DEP_FILES) : $(GENINCLUDES)

ifndef NO_DEPEND
-include $(DEP_FILES)
endif

