#ifndef _TRANSFORMS_H_
#define _TRANSFORMS_H_

extern void InitialiseStack(const unsigned int depth);

extern float* StackGetTop(void);
extern void StackPush(void);
extern void StackPop(void);
extern void StackIdentity(void);
extern void StackTranslate(const float, const float, const float);
extern void StackScale(const float, const float, const float);
extern void StackSetMatrix(const float mtx[16]);
extern void StackTransformMatrix(const float mtx[16]);

extern void DestroyStack();

#endif// _TRANSFORMS_H_