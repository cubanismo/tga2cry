#
# makefile for small machines + gcc
#

CC = gcc
CFLAGS = -O -Wall -m68020 -m68881
LIBS = -lpml
EXT = .ttp
OBJ = .o

tga2cry$(EXT):  tga2cry$(OBJ) cry$(OBJ) rgb$(OBJ) scale$(OBJ) palette$(OBJ)
	$(CC) -o tga2cry$(EXT) $(CFLAGS) tga2cry$(OBJ) cry$(OBJ) rgb$(OBJ) scale$(OBJ) palette$(OBJ) $(LIBS)

cry$(OBJ):	cry.s
	$(CC) -o cry$(OBJ) -c cry.s

tgainfo$(EXT): tgainfo$(OBJ)
	$(CC) -o tgainfo$(EXT) $(CFLAGS) tgainfo$(OBJ)
