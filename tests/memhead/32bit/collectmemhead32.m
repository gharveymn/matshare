min_bytes = 1;
max_bytes = 1000000;
teststride = 50;

chksz = 4;

ipts = (min_bytes:teststride:max_bytes)';
[X,Y] = meshgrid((1:chksz), ipts');

num_tests = numel(ipts);

res = zeros(num_tests, chksz);
for i = 1:num_tests
	res(i,:) = memhead(ipts(i));
end

rescomp = zeros(num_tests, chksz);
for i = 1:num_tests
	rescomp(i,:) = emumemhead(ipts(i));
end

if(sum(res(:) - rescomp(:)) ~= 0)
	disp('failed');
else
	disp('succeeded');
end
