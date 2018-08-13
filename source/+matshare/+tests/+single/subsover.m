% testing subscripted assignment
tv = rand(10,14);
matshare.share(tv);
f = matshare.fetch('-r');

tv(1) = 1;
f.data(1) = 1;

if(~isequal(f.data, tv))
	error('subscripted assignment failed');
end

tv(:) = 1:numel(tv);
f.data(:) = 1:numel(tv);

if(~isequal(f.data, tv))
	error('subscripted assignment failed');
end

tv(:,3) = 10:10+size(tv,1)-1;
f.data(:,3) = 10:10+size(tv,1)-1;

tv(7,:) = 10:10+size(tv,2)-1;
f.data(7,:) = 10:10+size(tv,2)-1;

if(~isequal(f.data, tv))
	error('subscripted assignment failed');
end

tv([9,4],:) = [20:20+size(tv,2)-1; 30:30+size(tv,2)-1];
f.data([9,4],:) = [20:20+size(tv,2)-1; 30:30+size(tv,2)-1];

if(~isequal(f.data, tv))
	error('subscripted assignment failed');
end

for i = 1:100000
	f.data([9,10,4],:) = rand(3,size(tv,2));
end
