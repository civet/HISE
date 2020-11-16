/*
BEGIN_TEST_DATA
  f: main
  ret: int
  args: int
  input: 12
  output: 1020
  error: ""
  filename: "init/init_test18"
END_TEST_DATA
*/

struct X
{
	void reset()
	{
		value *= 2;
	}
	
	template <int P> void setParameter(double v)
	{
		if(P == 0)
			value = (int)v;
	}
	

	int value = 8;
};

struct I
{
	I(X& x)
	{
		x.value = 12;
	}
};

#if 0
struct X2
{
	template <int P> void setParameter(double newValue)
	{
		v = (int)newValue;
	}
	

	int v = 9;
};

struct I2
{
	I2(X2& x2)
	{
		x2.v = 80;
	}
};
#endif

wrap::init<X, I> o;
//wrap::init<X2, I2> o2;

int main(int input)
{
  	o.setParameter<0>(20.0);
  	//o2.setParameter<0>(1000.0);
  	
	return o.obj.value;// + o2.obj.v;
}
