/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 * Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <openssl/rsa.h>

#include <string.h>

#include <openssl/bn.h>
#include <openssl/mem.h>
#include <openssl/err.h>

#include "internal.h"


#define BN_BLINDING_COUNTER 32

struct bn_blinding_st {
  BIGNUM *A;
  BIGNUM *Ai;
  BIGNUM *e;
  BIGNUM *mod;
  int counter;
  /* mont is the Montgomery context used for this |BN_BLINDING|. It is not
   * owned and must outlive this structure. */
  const BN_MONT_CTX *mont;
};

BN_BLINDING *BN_BLINDING_new(const BIGNUM *A, const BIGNUM *Ai, BIGNUM *mod) {
  BN_BLINDING *ret = NULL;

  ret = (BN_BLINDING*) OPENSSL_malloc(sizeof(BN_BLINDING));
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  memset(ret, 0, sizeof(BN_BLINDING));
  if (A != NULL) {
    ret->A = BN_dup(A);
    if (ret->A == NULL) {
      goto err;
    }
  }
  if (Ai != NULL) {
    ret->Ai = BN_dup(Ai);
    if (ret->Ai == NULL) {
      goto err;
    }
  }

  /* save a copy of mod in the BN_BLINDING structure */
  ret->mod = BN_dup(mod);
  if (ret->mod == NULL) {
    goto err;
  }
  if (BN_get_flags(mod, BN_FLG_CONSTTIME) != 0) {
    BN_set_flags(ret->mod, BN_FLG_CONSTTIME);
  }

  /* Set the counter to the special value -1
   * to indicate that this is never-used fresh blinding
   * that does not need updating before first use. */
  ret->counter = -1;
  return ret;

err:
  BN_BLINDING_free(ret);
  return NULL;
}

void BN_BLINDING_free(BN_BLINDING *r) {
  if (r == NULL) {
    return;
  }

  BN_free(r->A);
  BN_free(r->Ai);
  BN_free(r->e);
  BN_free(r->mod);
  OPENSSL_free(r);
}

int BN_BLINDING_update(BN_BLINDING *b, BN_CTX *ctx) {
  int ret = 0;

  if (b->A == NULL || b->Ai == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BN_NOT_INITIALIZED);
    goto err;
  }

  if (b->counter == -1) {
    b->counter = 0;
  }

  if (++b->counter == BN_BLINDING_COUNTER && b->e != NULL) {
    /* re-create blinding parameters */
    if (!BN_BLINDING_create_param(b, NULL, NULL, ctx, NULL)) {
      goto err;
    }
  } else {
    if (!BN_mod_mul(b->A, b->A, b->A, b->mod, ctx)) {
      goto err;
    }
    if (!BN_mod_mul(b->Ai, b->Ai, b->Ai, b->mod, ctx)) {
      goto err;
    }
  }

  ret = 1;

err:
  if (b->counter == BN_BLINDING_COUNTER) {
    b->counter = 0;
  }
  return ret;
}

int BN_BLINDING_convert(BIGNUM *n, BN_BLINDING *b, BN_CTX *ctx) {
  int ret = 1;

  if (b->A == NULL || b->Ai == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BN_NOT_INITIALIZED);
    return 0;
  }

  if (b->counter == -1) {
    /* Fresh blinding, doesn't need updating. */
    b->counter = 0;
  } else if (!BN_BLINDING_update(b, ctx)) {
    return 0;
  }

  if (!BN_mod_mul(n, n, b->A, b->mod, ctx)) {
    ret = 0;
  }

  return ret;
}

int BN_BLINDING_invert(BIGNUM *n, const BN_BLINDING *b, BN_CTX *ctx) {
  if (b->Ai == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BN_NOT_INITIALIZED);
    return 0;
  }
  return BN_mod_mul(n, n, b->Ai, b->mod, ctx);
}

BN_BLINDING *BN_BLINDING_create_param(
    BN_BLINDING *b, const BIGNUM *e, BIGNUM *m, BN_CTX *ctx,
    const BN_MONT_CTX *mont) {
  int retry_counter = 32;
  BN_BLINDING *ret = NULL;

  if (b == NULL) {
    ret = BN_BLINDING_new(NULL, NULL, m);
  } else {
    ret = b;
  }

  if (ret == NULL) {
    goto err;
  }

  if (ret->A == NULL && (ret->A = BN_new()) == NULL) {
    goto err;
  }
  if (ret->Ai == NULL && (ret->Ai = BN_new()) == NULL) {
    goto err;
  }

  if (e != NULL) {
    BN_free(ret->e);
    ret->e = BN_dup(e);
  }
  if (ret->e == NULL) {
    goto err;
  }

  if (mont != NULL) {
    ret->mont = mont;
  }

  do {
    if (!BN_rand_range(ret->A, ret->mod)) {
      goto err;
    }

    int no_inverse;
    if (BN_mod_inverse_ex(ret->Ai, &no_inverse, ret->A, ret->mod, ctx) == NULL) {
      /* this should almost never happen for good RSA keys */
      if (no_inverse) {
        if (retry_counter-- == 0) {
          OPENSSL_PUT_ERROR(RSA, RSA_R_TOO_MANY_ITERATIONS);
          goto err;
        }
        ERR_clear_error();
      } else {
        goto err;
      }
    } else {
      break;
    }
  } while (1);

  if (!BN_mod_exp_mont(ret->A, ret->A, ret->e, ret->mod, ctx, ret->mont)) {
    goto err;
  }

  return ret;

err:
  if (b == NULL) {
    BN_BLINDING_free(ret);
    ret = NULL;
  }

  return ret;
}

BN_BLINDING *rsa_setup_blinding(RSA *rsa, BN_CTX *in_ctx) {
  BIGNUM local_n;
  BIGNUM *n;
  BN_CTX *ctx;
  BN_BLINDING *ret = NULL;
  BN_MONT_CTX *mont_ctx = NULL;

  if (in_ctx == NULL) {
    ctx = BN_CTX_new();
    if (ctx == NULL) {
      return 0;
    }
  } else {
    ctx = in_ctx;
  }

  if (rsa->e == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_NO_PUBLIC_EXPONENT);
    goto err;
  }

  n = &local_n;
  BN_with_flags(n, rsa->n, BN_FLG_CONSTTIME);

  if (rsa->flags & RSA_FLAG_CACHE_PUBLIC) {
    mont_ctx = BN_MONT_CTX_set_locked(&rsa->mont_n, &rsa->lock, rsa->n, ctx);
    if (mont_ctx == NULL) {
      goto err;
    }
  }

  ret = BN_BLINDING_create_param(NULL, rsa->e, n, ctx, mont_ctx);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_BN_LIB);
    goto err;
  }

err:
  if (in_ctx == NULL) {
    BN_CTX_free(ctx);
  }

  return ret;
}
