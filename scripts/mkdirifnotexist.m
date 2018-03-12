function mkdirifnotexist(dirpath)
    if dirpath(end) ~= filesep, dirpath = [dirpath filesep]; end
    if (~exist(dirpath, 'dir')), mkdir(dirpath); end
end