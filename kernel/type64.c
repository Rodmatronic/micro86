#include <types.h>
#include <defs.h>
	
uint64_t
__udivmoddi4(uint64_t dividend, uint64_t divisor, uint64_t *remainder_ptr)
{
  uint64_t quotient = 0;
  uint64_t remainder = 0;
  
  if(divisor == 0) {
    if(remainder_ptr)
      *remainder_ptr = 0;
    return 0;
  }
  
  // Fast path for small divisors
  if(divisor > dividend) {
    if(remainder_ptr)
      *remainder_ptr = dividend;
    return 0;
  }
  
  if(divisor == dividend) {
    if(remainder_ptr)
      *remainder_ptr = 0;
    return 1;
  }
  
  for(int i = 63; i >= 0; i--) {
    remainder <<= 1;
    remainder |= (dividend >> i) & 1;
    
    if(remainder >= divisor) {
      remainder -= divisor;
      quotient |= (1ULL << i);
    }
  }
  
  if(remainder_ptr)
    *remainder_ptr = remainder;
  
  return quotient;
}

int64_t
__divmoddi4(int64_t dividend, int64_t divisor, int64_t *remainder_ptr)
{
  int dividend_negative = dividend < 0;
  int divisor_negative = divisor < 0;
  uint64_t abs_dividend = dividend_negative ? -dividend : dividend;
  uint64_t abs_divisor = divisor_negative ? -divisor : divisor;
  uint64_t abs_remainder;
  uint64_t abs_quotient = __udivmoddi4(abs_dividend, abs_divisor, &abs_remainder);
  int64_t quotient = abs_quotient;
  int64_t remainder = abs_remainder;
  
  if(dividend_negative != divisor_negative)
    quotient = -quotient;
  
  if(dividend_negative)
    remainder = -remainder;
  
  if(remainder_ptr)
    *remainder_ptr = remainder;
  
  return quotient;
}

int64_t
__moddi3(int64_t dividend, int64_t divisor)
{
  int64_t remainder;
  __divmoddi4(dividend, divisor, &remainder);
  return remainder;
}

uint64_t
__udivdi3(uint64_t dividend, uint64_t divisor)
{
        uint64_t quotient = 0;
        uint64_t remainder = 0;

        for(int i = 63; i >= 0; i--) {
                remainder <<= 1;
                remainder |= (dividend >> i) & 1;
                if(remainder >= divisor) {
                        remainder -= divisor;
                        quotient |= (1ULL << i);
                }
        }
        return quotient;
}

int64_t
__divdi3(int64_t dividend, int64_t divisor)
{
  return __divmoddi4(dividend, divisor, 0);
}
