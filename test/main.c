int division_test(void);
int buddy_test(void);

int main(int argc, char **argv)
{
  int ret = 0;
  ret = division_test() || ret;
  ret = buddy_test() || ret;
  return ret;
}
