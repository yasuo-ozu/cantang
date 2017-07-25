int i = 5, j = 0;
while (i) {
	i = i - 1;
	j = j + 1;
	if (i == 2) 
		break;
}
return j == 3;
