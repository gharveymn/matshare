fprintf('Testing subscripted overwriting... ');

% testing subscripted assignment
tv = rand(10,14);
f = matshare.share(tv);

tv = testsubs(tv, f, substruct('()',{1}));
tv = testsubs(tv, f, substruct('()',{':'}));
tv = testsubs(tv, f, substruct('()',{':',3}));
tv = testsubs(tv, f, substruct('()',{7, ':'}));
tv = testsubs(tv, f, substruct('()',{[9,4], ':'}));
tv = testsubs(tv, f, substruct('()',{[7,3,2],1,1,1,':',':',1,1,ones(3),[1;1;1],':',':',':'}));
tv = testsubs(tv, f, substruct('()',{':'}), @(x) randi(255, x, 'uint8'));

clear tv

tv(1) = struct('f1', rand(3),  'f2', [9,565,312345]);
tv(2) = struct('f1', rand(10), 'f2', [1,2,3]);
tv(3) = struct('f1', 2,        'f2', rand(7));
tv(4) = struct('f1', rand(3),  'f2', [5,23,3123]);

f = matshare.share(tv);

replacement(1) = struct('f1', magic(10), 'f2', [4,5,6]);
replacement(2) = struct('f1', 9,         'f2', wilkinson(7));

tv(2:3) = replacement;
f.data(2:3) = replacement;

if(~matshare.scripts.compstruct(tv, f.data))
	error('Subscripted assignment failed because results were not equal.');
end

clear tv

tv = {1,2,3,4,5};
f = matshare.share(tv);

tv(2:4) = {6,7,8};
f.data(2:4) = {6,7,8};

if(~matshare.scripts.compstruct(tv, f.data))
	error('Subscripted assignment failed because results were not equal.');
end

clear tv

g = [struct('g1',rand(2),'g2',rand(10)); struct('g1',[],'g2','adfa')];
g(2).g1 = {1,4,1};

tv(1) = struct('f1', rand(3),         'f2', [1,2,3]);
tv(2) = struct('f1', randi(13,[4,3]), 'f2', randi(13,[4,3,7], 'uint8'));
tv(3) = struct('f1', g,               'f2', []);
tv(4) = struct('f1', [],              'f2', 1);

tv(3).f2 = {1,7,rand(3)};

f = matshare.share(tv);

% testing through idiomatic calls; using substruct may not cover search space
tv(3).f1(2).g1{2} = 17;
f.data(3).f1(2).g1{2} = tv(3).f1(2).g1{2};

if(~matshare.scripts.compstruct(tv, f.data))
	error('Subscripted assignment failed because results were not equal.');
end

tv(3).f1(1).g2(4:7,2:8) = rand(size(tv(3).f1(1).g2(4:7,2:8)));
f.data(3).f1(1).g2(4:7,2:8) = tv(3).f1(1).g2(4:7,2:8);

if(~matshare.scripts.compstruct(tv, f.data))
	error('Subscripted assignment failed because results were not equal.');
end

% tv = {rand(5), tv, 3,['asdfa';'ewrwe']};
% 
% f = matshare.share(tv);

fprintf('Test successful.\n\n');

