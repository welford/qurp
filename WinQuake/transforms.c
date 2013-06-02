#ifndef _TRANSFORMS_H_
#define _TRANSFORMS_H_

#include "vector.h"
#include <stdlib.h>
//#include <string.h>

typedef struct _MatrixStack{
	unsigned int	idx;
	unsigned int	size;
	Matrix44		*pStack;
}MatrixStack;

MatrixStack stack;

float* StackGetTop(void){
	return stack.pStack[stack.idx].a;
	
}

void StackPush(void){
	if(stack.idx+1 < stack.size){
		stack.pStack[stack.idx+1] = stack.pStack[stack.idx];
		stack.idx++;
	}
}

void StackPop(void){
	if(stack.idx > 0)
		stack.idx -= 1;
}

void StackIdentity(void){
	matSetIdentity44( &stack.pStack[stack.idx] );
}

void StackTranslate(const float x, const float y, const float z){
	matMatrixTranslate44( x, y, z, &stack.pStack[stack.idx] );
}

void StackScale(const float x, const float y, const float z){
	matScaleXYZ44(x, y, z, &stack.pStack[stack.idx]);
}

void StackSetMatrix(const float mtx[16]){
	unsigned int  i=0;
	for(i=0;i<16;i++){
		stack.pStack[stack.idx].a[i] = mtx[i];
	}
}

void StackTransformMatrix(const float mtx[16]){
	matMultiply44((Matrix44*)mtx, &stack.pStack[stack.idx], &stack.pStack[stack.idx]);
}

void InitialiseStack(const unsigned int depth){
	stack.idx = 0;
	stack.size = depth;
	stack.pStack = (Matrix44*) malloc(sizeof(Matrix44) * depth);
	Identity();
}

extern void DestroyStack(){
	stack.idx = 0;
	stack.size = 0;
	free(stack.pStack);
}

#endif// _TRANSFORMS_H_