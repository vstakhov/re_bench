RE2_LIB=/opt/local/lib
RE2_INC=/opt/local/include

PCRE_LIB=/opt/pcre/lib
PCRE_INC=/opt/pcre/include

PCRE2_LIB=/opt/pcre2/lib
PCRE2_INC=/opt/pcre2/include
HYPERSCAN_LIB=/opt/hyperscan/lib
HYPERSCAN_INC=/opt/hyperscan/include

CFLAGS= -c -Wall -Werror -O3 -g -I/opt/local/include
CXXFLAGS= -c -Wall -Werror -O3 -g
LDFLAGS= -L/opt/local/lib
REGEX1=
FILE_ABC=abc.txt
FILE_RAND_ABC=rand-abc.txt
FILE_DELIM=delim.txt
FILE_MTENT12=mtent12.txt

ifneq (Darwin,$(shell uname -s))
    LDFLAGS+=-lrt
endif

.PHONY: all
all: sregex pcre pcre2 re2 hyperscan

sregex: sregex.o ../libsregex.a
	$(CC) -o $@ -Wl,-rpath,.. -L.. $< -lsregex $(LDFLAGS)

re2: re2.o
	$(CXX) -o $@ -Wl,-rpath,$(RE2_LIB) -L$(RE2_LIB) $< -lre2 $(LDFLAGS)

pcre: pcre.o
	$(CC) -o $@ -Wl,-rpath,$(PCRE_LIB) -L$(PCRE_LIB) $< -lpcre $(LDFLAGS)

pcre2: pcre2.o
	$(CC) -o $@ -Wl,-rpath,$(PCRE2_LIB) -L$(PCRE2_LIB) $< -lpcre2-8 $(LDFLAGS)

hyperscan: hyperscan.o
	$(CXX) -o $@ -Wl,-rpath,$(HYPERSCAN_LIB) -L$(HYPERSCAN_LIB) -lhs  $(LDFLAGS) $<

%.o: %.c
	$(CC) $(CFLAGS) -I../src -I$(RE1_INC) -I$(PCRE_INC) -I$(PCRE2_INC) -I$(HYPERSCAN_INC) $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -I$(RE2_INC) $<

$(FILE_MTENT12):
	wget http://agentzh.org/misc/re/bench/$(FILE_MTENT12)

.PHONY: bench
bench: all $(FILE_ABC) $(FILE_RAND_ABC) $(FILE_DELIM) $(FILE_MTENT12)
	./bench 'd' $(FILE_ABC)
	./bench 'd|de' $(FILE_RAND_ABC)
	./bench 'dfa|efa|ufa|zfa' $(FILE_ABC)  # 16.6ms
	./bench 'dfa|efa|ufa|zfa' mtent12.txt
	./bench '[d-z]' $(FILE_ABC)
	./bench '[d-hx-z]' $(FILE_ABC)
	./bench '[d-hx-z]' mtent12.txt  # 63.04006
	./bench 'ddd|fff|eee|ggg|hhh|iii|jjj|kkk|[l-n]mm|ooo|ppp|qqq|rrr|sss|ttt|uuu|vvv|www|[x-z]yy' mtent12.txt #$(FILE_ABC)  # 126.75379ms
	./bench 'ddd|fff|eee|ggg|hhh|iii|jjj|kkk|[l-n]mm|ooo|ppp|qqq|rrr|sss|ttt|uuu|vvv|www|[x-z]yy' $(FILE_ABC)  # 9.92087ms
	./bench '(?:a|b)aa(?:aa|bb)cc(?:a|b)' $(FILE_ABC)
	./bench '(?:a|b)aa(?:aa|bb)cc(?:a|b)abcabcabd' $(FILE_RAND_ABC)
	./bench '(a|b)aa(aa|bb)cc(a|b)abcabcabc' $(FILE_ABC)
	./bench '(a|b)aa(aa|bb)cc(a|b)abcabcabc' $(FILE_RAND_ABC) # 51.12535ms
	./bench 'd[abc]*?d' $(FILE_DELIM)  # 5.04849ms
	./bench 'd.*?d' $(FILE_DELIM)  # 0.52372ms
	./bench 'd.*d' $(FILE_DELIM)	# 0.54214ms (slow than PCRE interp)
	./bench 'Twain' mtent12.txt  # 1.80803ms (1.74212ms)
	./bench '(?i)Twain' mtent12.txt   # 19.81792ms (18.70ms)
	./bench '[a-z]shing' mtent12.txt  # 46.61745ms
	./bench 'Huck[a-zA-Z]+|Saw[a-zA-Z]+' mtent12.txt # 8.51685ms WE SLOW! (11.64507ms)
	./bench '\b\w+nn\b' mtent12.txt # 52.90915ms (49.79010ms) (55.07292) (51.82008)
	./bench 'Tom|Sawyer|Huckleberry|Finn' mtent12.txt  # WE SLOW!!  43.73897ms (18.01813ms)
	./bench '(?i)Tom|Sawyer|Huckleberry|Finn' mtent12.txt # WE SLOW!!  99.46298 (66.39118ms)
	./bench '.{0,2}(Tom|Sawyer|Huckleberry|Finn)' mtent12.txt  # 44.44251ms (18.11103ms)
	./bench '.{2,4}(Tom|Sawyer|Huckleberry|Finn)' mtent12.txt  # 45.11014ms (18.35325ms)
	./bench 'Tom.{10,25}river|river.{10,25}Tom' mtent12.txt gcc # WE SLOW!!
	./bench '[a-zA-Z]+ing' mtent12.txt  # 52.70686ms (56.65410ms)
	./bench '\s[a-zA-Z]{0,12}ing\s' mtent12.txt  # 71.74556ms
	./bench '([A-Za-z]awyer|[A-Za-z]inn)\s' mtent12.txt  # 61.39171ms
	./bench $$'["\'][^"\']{0,30}[?!\.]["\']' mtent12.txt  # 13.57093ms

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
	./gen-plot.pl a.txt
