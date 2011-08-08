RONN ?= ronn --organization="IITiS PAN Gliwice" --pipe

default: all
all: \
	out/iitis-generator.1.man out/iitis-generator.1.html \
	out/iitis-generator-conf.5.man out/iitis-generator-conf.5.html \
	out/iitis-generator-output.5.man out/iitis-generator-output.5.html \
	out/iitis-generator-traffic.5.man out/iitis-generator-traffic.5.html

index.txt: *.markdown
	./mkindex.sh

out/%.man : %.markdown index.txt
	mkdir -p out
	$(RONN) --roff $< > $@

out/%.html : %.markdown index.txt
	mkdir -p out
	$(RONN) --html --style=toc $< > $@

.PHONY: clean
clean:
	-rm -f out/*
