/*
BEGIN_TEST_DATA
  f: main
  ret: int
  args: int
  input: 12
  output: 3
  error: ""
  filename: "dyn/dyn_subscript"
END_TEST_DATA
*/

span<int, 5> c = { 1, 2, 3, 4, 5 };
dyn<int> x;

int main(int input)
{
  dyn<int>::wrapped i = 2;
	x.referTo(c);

  return x[i];
}

