SUB_DIRS:=log pthreadpool work main obj
export SUB_DIRS

all:$(SUB_DIRS)

$(SUB_DIRS):MK_bin
	make -C $@

MK_bin:
	mkdir -p ./bin

.PHONY:clean
clean:
	rm -rf ./obj/*.o bin/server