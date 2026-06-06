TARGET := report.pdf

.PHONY: all clean

all: $(TARGET)

%.pdf: %.tex
	latexmk -xelatex -pv- $(EXTRAFLAGS) -aux-directory=build -jobname=report $<

sync: EXTRAFLAGS+=-synctex=1 -interaction=nonstopmode
sync: $(TARGET)

.PHONY: clean
clean:
	rm -rf ./build ./report.pdf ./report.synctex.gz
