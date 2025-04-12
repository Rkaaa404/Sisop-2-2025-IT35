# Command line usage
starterkit --decrypt
starterkit --quarantine
starterkit --shutdown
starterkit --return
starterkit --eradicate

# Build

## 1. Install dependency
### ubuntu
```
apt install 
```

### arch linux
```
pacman -S base-devel
```

## 2. Finally build the program
```
gcc starterkit.c -o starterkit -lpthread
```

# Development POC
using starterkit.py can be later converted to C program
due to faster development solution and later porting c
specific problem

for working dir we use tmp which can be used for testing
