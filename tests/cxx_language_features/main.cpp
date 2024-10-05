
#include "enum.h"
#include "template.h"

class IncompleteType; // declaration

int uninitializedGlobalVariable;  // definition
static int uninitializedStaticGlobalVariable; // definition
extern int uninitializedExternGlobalVariable; // declaration

void notMain(int argc, char* argv[]) {
  char c = *argv[argc - 1];
}

int main() { }
