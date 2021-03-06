SRCS = main.c global.c physics.c random.c read_args.c render.c
ODIR = objs
OBJS = $(patsubst %,$(ODIR)/%,$(SRCS:.c=.o))
DEPS = $(OBJS:.o=.d)
TARGET = exe
OUTPUT = animation.gif
PROFDIR = profile

CC = clang-10
WFLAGS = -Werror -Wall -Wextra -Wwrite-strings -Wshadow
CFLAGS = -flto -march=native -ffast-math -D THREAD_COUNT=$$(grep -c ^processor /proc/cpuinfo)
LFLAGS = $(CFLAGS)
LIBS = -lm -fopenmp
OFLAGS = -O3 -DNTEST -DNDEBUG -fopenmp # -DNRENDER -DNBENCH
DFLAGS = -O2 -g3 -fno-omit-frame-pointer -DNTEST
DLFLAGS = $(DFLAGS) -fno-pic -no-pie

ASAN = -fsanitize=address -fsanitize=leak -fsanitize=undefined
MSAN = -fsanitize=memory -fsanitize=undefined
TSAN = -fsanitize=thread -fsanitize=undefined


-include $(DEPS)

-include $(TARGET).d

# Compile
$(ODIR)/%.o: %.c
	$(CC) -MMD $(WFLAGS) $(CFLAGS) $(OFLAGS) -c $< -o $@

#Link
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(WFLAGS) $(LFLAGS) $(LIBS) $(OFLAGS)

$(OUTPUT): $(TARGET)
	$(MAKE) run


.PHONY = all clean bench debug show run run_small run_test

all: $(TARGET)

clean:
	$(RM) $(TARGET) $(TARGET).d $(DEPS) $(OBJS) $(OUTPUT) $(ODIR)/*.opt.yaml profile/*.opt.yaml

debug:
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(DFLAGS) -o $(TARGET) $(WFLAGS) $(DLFLAGS) $(LIBS) $(CFLAGS)

build_asan:
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(DFLAGS) $(ASAN) -o $(TARGET) $(WFLAGS) $(DLFLAGS) $(LIBS) $(CFLAGS)

build_msan:
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(DFLAGS) $(MSAN) -o $(TARGET) $(WFLAGS) $(DLFLAGS) $(LIBS) $(CFLAGS)

build_tsan:
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(DFLAGS) $(TSAN) -o $(TARGET) $(WFLAGS) $(DLFLAGS) $(LIBS) $(CFLAGS)

run: $(TARGET)
	./$(TARGET) --n=1682 --time=10.0 --box-radius=29.0 --avg-speed=1.0 --ups=1000.0 --fps=50.0 --resolution=480 --output-file=$(OUTPUT)

run_small: $(TARGET)
	./$(TARGET) --n=200 --time=1.0 --box-radius=10.0 --avg-speed=1.0 --ups=200.0 --fps=24.0 --resolution=240 --output-file=$(OUTPUT)

sample: $(TARGET)
	./$(TARGET) --n=968 --time=10.0 --box-radius=16.0 --avg-speed=1.6 --ups=400.0 --fps=50.0 --resolution=360 --output-file=$(OUTPUT)
	convert $(OUTPUT) samples/sample.gif
	ls -lah samples/sample.gif

time:
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(OFLAGS) -Rpass-analysis=loop-vectorize -fsave-optimization-record -DNRENDER -o $(TARGET) $(WFLAGS) $(LFLAGS) $(LIBS) $(CFLAGS)
	mv *.opt.yaml $(ODIR)/
	./$(TARGET) --n=1682 --time=100.0 --box-radius=29.0 --avg-speed=1.0 --ups=1000.0 --fps=50.0 --resolution=480 --output-file=$(OUTPUT)

bench: $(PROFDIR)
	$(CC) $(SRCS) $(WFLAGS) $(CFLAGS) $(DFLAGS) -DNRENDER -o $(TARGET) $(WFLAGS) $(DLFLAGS) $(LIBS)
	valgrind --tool=callgrind --callgrind-out-file=$(PROFDIR)/latest.out ./$(TARGET) --n=1682 --time=1.0 --box-radius=29.0 --avg-speed=1.0 --ups=100.0 --fps=50.0 --resolution=100 --output-file=animation.gif
	callgrind_annotate --auto=yes $(PROFDIR)/latest.out > $(PROFDIR)/latest.log
	code $(PROFDIR)/latest.log

show: $(OUTPUT)
	code $(OUTPUT)

run_test: $(TARGET)
	./$(TARGET) --n=968 --time=30.0 --box-radius=20.0 --avg-speed=0.6 --ups=100.0 --fps=50.0 --resolution=300 --output-file=$(OUTPUT)
