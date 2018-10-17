/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  * Additional permission under GNU GPL version 3 section 7
  *
  * If you modify this Program, or any covered work, by linking or combining
  * it with OpenSSL (or a modified version of that library), containing parts
  * covered by the terms of OpenSSL License and SSLeay License, the licensors
  * of this Program grant you additional permission to convey the resulting work.
  *
  */

extern "C"
{
#include "c_groestl.h"
#include "c_blake256.h"
#include "c_jh.h"
#include "c_skein.h"
}
#include "xmrstak/backend/cryptonight.hpp"
#include "cryptonight.h"
#include "cryptonight_altivec.h"
#include "xmrstak/misc/console.hpp"
#include "xmrstak/jconf.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include <malloc.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
void do_blake_hash(const void* input, uint32_t len, char* output) {
	blake256_hash((uint8_t*)output, (const uint8_t*)input, len);
}

void do_groestl_hash(const void* input, uint32_t len, char* output) {
	groestl((const uint8_t*)input, len * 8, (uint8_t*)output);
}

void do_jh_hash(const void* input, uint32_t len, char* output) {
	jh_hash(32 * 8, (const uint8_t*)input, 8 * len, (uint8_t*)output);
}

void do_skein_hash(const void* input, uint32_t len, char* output) {
	skein_hash(8 * 32, (const uint8_t*)input, 8 * len, (uint8_t*)output);
}

void (* const extra_hashes[4])(const void *, uint32_t, char *) = {do_blake_hash, do_groestl_hash, do_jh_hash, do_skein_hash};

size_t cryptonight_init(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg)
{
	return 1;
}

cryptonight_ctx* cryptonight_alloc_ctx(size_t use_fast_mem, size_t use_mlock, alloc_msg* msg)
{
	size_t hashMemSize = std::max(
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgo()),
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgoRoot())
	);

	cryptonight_ctx* ptr = (cryptonight_ctx*)malloc(sizeof(cryptonight_ctx));

	if(use_fast_mem == 0)
	{
		// use 2MiB aligned memory
		ptr->long_state = (uint8_t*)malloc(hashMemSize);
		ptr->ctx_info[0] = 0;
		ptr->ctx_info[1] = 0;
		if(ptr->long_state == NULL)
			printer::inst()->print_msg(L0, "MEMORY ALLOC FAILED: _mm_malloc was not able to allocate %s byte",std::to_string(hashMemSize).c_str());
		return ptr;
	}

	ptr->long_state = (uint8_t*)mmap(NULL, hashMemSize, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);

	if (ptr->long_state == MAP_FAILED)
	{
		free(ptr);
		msg->warning = "mmap failed, check attribute 'use_slow_memory' in 'config.txt'";
		return NULL;
	}

	ptr->ctx_info[0] = 1;

	if(madvise(ptr->long_state, hashMemSize, MADV_RANDOM|MADV_WILLNEED) != 0)
		msg->warning = "madvise failed";

	ptr->ctx_info[1] = 0;
	if(use_mlock != 0 && mlock(ptr->long_state, hashMemSize) != 0)
		msg->warning = "mlock failed";
	else
		ptr->ctx_info[1] = 1;

	return ptr;
}

void cryptonight_free_ctx(cryptonight_ctx* ctx)
{
	size_t hashMemSize = std::max(
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgo()),
		cn_select_memory(::jconf::inst()->GetCurrentCoinSelection().GetDescription(1).GetMiningAlgoRoot())
	);

	if(ctx->ctx_info[0] != 0)
	{
		if(ctx->ctx_info[1] != 0)
			munlock(ctx->long_state, hashMemSize);
		munmap(ctx->long_state, hashMemSize);
	}
	else
	free(ctx->long_state);

	free(ctx);
}
