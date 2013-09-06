
test:
	clang++ -std=c++11 -g lexer.cc test_lexer.cc -o test_lexer
	clang++ -std=c++11 -g lexer.cc ast.cc llparser.cc test_parser.cc \
		-rdynamic `llvm-config --cppflags --ldflags --libs core jit native` \
		-o test_parser

clean:
	rm -f *.o test_parser test_lexer
