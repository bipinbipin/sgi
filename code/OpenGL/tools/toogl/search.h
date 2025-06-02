

const NALPH = 128;

const MAXPATTERN = 32;



class Search {
    public:
	Search() ;
	~Search() {} ;
	void add(const char *s) ;
	void print_table() ;
	int check(const char *s) ;
    private:
	unsigned int table[NALPH];
	unsigned int cstate;
	unsigned int lim;
};
