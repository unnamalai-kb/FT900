
OUTDIR = S
INDIR = .
RELDIR = ../Certificates

CRT_SRCS += \
rootca.pem \
ft900device1_cert.pem \
ft900device1_pkey.pem

CRT_S += \
$(OUTDIR)/rootca.S \
$(OUTDIR)/ft900device1_cert.S \
$(OUTDIR)/ft900device1_pkey.S



#-------------------------------------------------------------------------------
# All targets
#-------------------------------------------------------------------------------
.PHONY: all
all: clean dirs $(CRT_S)

dirs: $(OUTDIR)

$(OUTDIR):
	@echo 'Creating directory: $<'
	mkdir $(OUTDIR)

# Each subdirectory must supply rules for building sources it contributes
$(OUTDIR)/%.S: $(INDIR)/%.pem
	@echo 'Building file: $<'
	@echo 'Making S file: $@'
	$(eval SYMNAME = $(subst .,_, $(notdir $<)))
	@echo 'Symbol: $(SYMNAME)'
	@echo ### Auto-generated file by certificates.mak ### > $@
	@echo .global $(SYMNAME) >> $@
	@echo $(SYMNAME): >> $@
	@echo .incbin "$(RELDIR)/$<" >> $@
	@echo .byte 0 >> $@
	@echo .global $(SYMNAME)_end >> $@
	@echo $(SYMNAME)_end: >> $@
	@echo 'Finished making file: $@'
	@echo ' '
	sleep 2

.PHONY: clean
clean:
	-$(RM) $(CRT_S) 
	-@echo ' '
