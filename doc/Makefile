RONN ?= ronn --organization="IITiS PAN Gliwice" --pipe

default: all
all: out/iitis-generator.1.man out/iitis-generator.1.html

index.txt: *.markdown
	./mkindex.sh > index.txt

out/%.man : %.markdown index.txt
	mkdir -p out
	$(RONN) --roff $< > $@

out/%.html : %.markdown index.txt
	mkdir -p out
	$(RONN) --html --style=toc $< > $@

.PHONY: clean
clean:
	-rm -f out/*
