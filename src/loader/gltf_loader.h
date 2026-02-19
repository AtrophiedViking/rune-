#pragma once
#include <string>

struct State;
//Loading
void modelLoad(State* state, std::string modelPath);
void modelUnload(State* state);