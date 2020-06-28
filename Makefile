CFLAGS = -Wall
OBJ = .o
OBJS = tga2cry$(OBJ) cry$(OBJ) rgb$(OBJ) scale$(OBJ) palette$(OBJ)
LDFLAGS = -lm
EXT =

tga2cry$(EXT): $(OBJS)
	$(CC) -o tga2cry$(EXT) $(CFLAGS) $(LDFLAGS) $^

#cry$(OBJ): cry.s
#	$(CC) -o cry$(OBJ) -c cry.s

tgainfo$(EXT): tgainfo$(OBJ)
	$(CC) -o tgainfo$(EXT) $(CFLAGS) tgainfo$(OBJ)
