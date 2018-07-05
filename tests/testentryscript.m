x = 1;
y = 2;
z = 3;
w = 4;

tic
for i = 1:100000
	%testentry(x);
	%testentry(y);
	%testentry(z);
	%testentry(w);
	testentry(x,y,z,w);
	%testentry({x,y,z,w});
end
toc