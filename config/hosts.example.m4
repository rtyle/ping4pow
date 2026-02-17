dnf comment
dnf add a third argument for a host that we must power-cycle when not pingable;
dnf otherwise, we will wait for all to be pingable before power-cycling.
host(10.1.1.1, A)
host(10.1.1.2, B)
host(10.1.1.3, C)
