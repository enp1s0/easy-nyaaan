easy-nyaaan: source.cpp 
	g++ $< -std=c++11 -lboost_regex -lcurl -loauth -o $@
