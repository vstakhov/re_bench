RE2_LIB=/opt/re2/lib
RE2_INC=/opt/re2/include

RE1_LIB=../../re1
RE1_INC=../../re1

PCRE_LIB=/opt/pcre/lib
PCRE_INC=/opt/pcre/include

PCRE2_LIB=/opt/pcre2/lib
PCRE2_INC=/opt/pcre2/include

CFLAGS= -c -Wall -Werror -O3 -g
CXXFLAGS= -c -Wall -Werror -O3 -g
LDFLAGS=
REGEX1=
FILE_ABC=abc.txt
FILE_RAND_ABC=rand-abc.txt
FILE_DELIM=delim.txt

ifneq (Darwin,$(shell uname -s))
    LDFLAGS+=-lrt
endif

.PHONY: all
all: sregex re1 pcre pcre2 re2

sregex: sregex.o ../libsregex.a
	$(CC) -o $@ -Wl,-rpath,.. -L.. $< -lsregex $(LDFLAGS)

re1: re1.o $(RE1_LIB)/libre1.a
	$(CC) -o $@ -Wl,-rpath,$(RE1_LIB) -L$(RE1_LIB) $< -lre1 $(LDFLAGS)

re2: re2.o
	$(CXX) -o $@ -Wl,-rpath,$(RE2_LIB) -L$(RE2_LIB) $< -lre2 $(LDFLAGS)

pcre: pcre.o
	$(CC) -o $@ -Wl,-rpath,$(PCRE_LIB) -L$(PCRE_LIB) $< -lpcre $(LDFLAGS)

pcre2: pcre2.o
	$(CC) -o $@ -Wl,-rpath,$(PCRE2_LIB) -L$(PCRE2_LIB) $< -lpcre2-8 $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I../src -I$(RE1_INC) -I$(PCRE_INC) -I$(PCRE2_INC) $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -I$(RE2_INC) $<

.PHONY: bench
bench: all $(FILE_ABC) $(FILE_RAND_ABC) $(FILE_DELIM)
	#./bench '(?:a|b)aa(?:aa|bb)cc(?:a|b)' $(FILE_ABC)
	#./bench '(?:a|b)aa(?:aa|bb)cc(?:a|b)abcabcabd' $(FILE_RAND_ABC)
	#@echo "========================================"
	#./bench '(a|b)aa(aa|bb)cc(a|b)abcabcabc' $(FILE_ABC)
	#@echo "========================================"
	#./bench '(a|b)aa(aa|bb)cc(a|b)abcabcabc' $(FILE_RAND_ABC)
	#./bench '(a|b)aa(aa|bb)cc(a|b)abcabcabc' $(FILE_ABC)
	#./bench 'dfa|efa|ffa|gfa' $(FILE_ABC)
	#./bench '[d-h]' $(FILE_ABC)
	#./bench 'dd|ff|ee|gg|hh|ii|jj|kk|[l-n]m|oo|pp|qq|rr|ss|tt|uu|vv|ww|[x-z]y' $(FILE_ABC)
	#./bench 'd|de' $(FILE_RAND_ABC)
	#./bench 'd[abc]*?d' $(FILE_DELIM)
	#./bench 'd.*?d' $(FILE_DELIM)
	./bench 'd.*d' $(FILE_DELIM)
	#./bench 'd' $(FILE_ABC)
	#./bench '[a-zA-Z]+ing' mtent12.txt
	#./bench '\s[a-zA-Z]{0,12}ing\s' mtent12.txt
	#./bench 'Twain' mtent12.txt
	#./bench '(?i)Twain' mtent12.txt
	#./bench '[a-z]shing' mtent12.txt
	#./bench 'Huck[a-zA-Z]+|Saw[a-zA-Z]+' mtent12.txt
	#./bench '\b\w+nn\b' mtent12.txt
	#./bench '[a-q][^u-z]{13}x' mtent12.txt

clean:
	rm -rf *.o sregex re1

$(FILE_ABC):
	perl gen/abc.pl

$(FILE_RAND_ABC):
	perl gen/rand-abc.pl

$(FILE_DELIM):
	perl gen/delim.pl

.PHONY: plot
plot:
	$(MAKE) bench > a.txt
	perl gen-csv.pl a.txt > a.csv
	gnuplot bench.gnu
