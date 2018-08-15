% testing subscripted assignment
tv = rand(10,14);
matshare.share(tv);
f = matshare.fetch('-r');

tv = testsubs(tv, f, {1});
tv = testsubs(tv, f, {':'});
tv = testsubs(tv, f, {':',3});
tv = testsubs(tv, f, {7, ':'});
tv = testsubs(tv, f, {[9,4], ':'});
tv = testsubs(tv, f, {[7,3,2],1,1,1,':',':',1,1,ones(3),[1;1;1],':',':',':'});
tv = testsubs(tv, f, {':'}, @(x) randi(255, x, 'uint8'));
