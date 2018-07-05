tic
for i = 1:10000
	x = testclass(1,2);
	[y,z] = x.func(3,4);
end
toc

tic
for i = 1:10000
	x = testclass(1,2);
	[y,z] = x.func(3,4);
end
toc