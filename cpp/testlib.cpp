#include <stdio.h>
#include <dlfcn.h>

int main ()
{	
	if (dlopen("libndnrtc.dylib",RTLD_LAZY) == NULL)
		printf("error while loading: %s \n", dlerror());
	else
		printf("successfully loaded\n");
		
	return 0;
}