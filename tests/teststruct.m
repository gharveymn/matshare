clear a
a = MatShare;
load(fullfile(pwd,'res','my_struct.mat'));
a.shared.data = my_struct;
a.shared.data