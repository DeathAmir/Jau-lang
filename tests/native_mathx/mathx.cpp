extern "C" long long mx_add(long long a,long long b){return a+b;}
extern "C" long long mx_mul(long long a,long long b){return a*b;}
extern "C" long long mx_pow(long long base,long long exp){long long r=1;for(long long i=0;i<exp;++i)r*=base;return r;}
