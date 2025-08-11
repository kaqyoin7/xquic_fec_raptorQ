#pragma once
#include "Helper.h"
#include "Symbol.h"
// 负责丢包情况下的数据恢复（解码）,内部同样依赖 Generators

Decoder::Decoder()
{
}

Decoder::~Decoder()
{
	delete gen;
}

/* initialize general parameters as much as possible
   this is meant to be called only once.
   By default, N is set to K. It will be fixup in prepare()
 */
// 初始化参数
bool Decoder::init(int K, int T)
{
	gen = new Generators();
	return gen->gen(K, K, T);
}
// 解码接收到的符号，生成中间符号
Symbol **Decoder::decode(char **source, int _N, int *esi)
{
	Symbol **s;

	gen->prepare(source, _N, esi);
	s = gen->generate_intermediates();
	if (!s)
		return NULL;
	return s;
}
// 恢复丢失的第 x 个符号
Symbol *Decoder::recover(int x)
{
	return gen->recover_symbol(x);
}
