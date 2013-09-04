
test:
	clang++ -std=c++11 -g lexer.cc test_lexer.cc -o test_lexer
	clang++ -std=c++11 -g lexer.cc ast.cc llparser.cc test_llparser.cc \
		-rdynamic `llvm-config --cppflags --ldflags --libs core jit native` \
		-o test_llparser

clean:
	rm -f *.o test_llparser test_lexer
