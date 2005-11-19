#define BIT_FIELD(size, pos) (((pos) << 6) | size)
#define BIT_POS(ppss) (((ppss) >> 6) & 077)
#define BIT_SIZE(ppss) ((ppss) & 077)

struct sym {
	char *name;
	unsigned int val;
};
