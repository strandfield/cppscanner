
#include "palindrome.h"
#include "print.h"

int main(int argc, char* argv[])
{
  println("Hello!");

  std::string text = "aba";
  print(isPalindrome(text) ? text + " is a palindrome" : "there is a problem with this program");

  return 0;
}
