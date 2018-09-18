n = 500;
A = 500;

matshare.clear;

tic
a = matshare.share('a',zeros(n));
parfor i = 1:n
	pa = matshare.fetch;
	v = abs(eig(rand(A)));
	pa.overwrite(v(1:n), substruct('()',{':',i}));
end
toc

tic
a = zeros(n);
parfor i = 1:n
	v = abs(eig(rand(A)));
	a(:,i) = v(1:n);
end
toc

tic
a = zeros(n);
for i = 1:n
	v = abs(eig(rand(A)));
	a(:,i) = v(1:n);
end
toc



