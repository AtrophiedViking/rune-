#pragma once
struct State;

//Error Handlin
void logPrint();
void exitCallback();
void glfwErrorCallback(int errorCode, const char* description);
void errorHandlingSetup();
//utility
void callbackSetup(State* state);

//Window
void initGLFW(bool windowResizable);

void surfaceCreate(State* state);
void surfaceDestroy(State* state);

void windowCreate(State* state);
void windowDestroy(State* state);

//Draw
void frameDraw(State* state);
void updateFPS(State* state);