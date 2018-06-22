tic
for i = 1:1000000
	x = int32(randi(100));
end
toc

tic
for i = 1:1000000
	x = int64(randi(100));
end
toc

tic
for i = 1:1000000
	x = double(randi(100));
end
toc