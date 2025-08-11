#include "Helper.h"
#include "Symbol.h"
// 负责源数据的编码
// 内部依赖 Generators 生成编码矩阵和符号
Encoder::Encoder()
{
}

Encoder::~Encoder()
{
	delete gen;
}
// 初始化参数
bool Encoder::init(int K, int T)
{
	gen = new Generators();
	return gen->gen(K, K, T);
}
// 对源数据编码，生成冗余修复符号
Symbol **Encoder::encode(char **source, int overhead)
{
	Symbol **s;

	gen->prepare(source, gen->getK(), NULL);
	s = gen->generate_intermediates();
	if (!s)
		return NULL;
	return gen->generate_repairs(overhead);
}
