#include "board.hpp"

Board::Board(){
    Board("http", "2048.semantics3.com", "80");
}

Board::Board(std::string const protocol, std::string const endpoint, std::string const port) : endpoint(endpoint), seed_gen(), rand4(0,3), rand10(0,9) {
    std::mt19937 mt(seed_gen());
    this->mt = mt;
    
    struct addrinfo addr;
    addr.ai_family = AF_UNSPEC;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_flags = 0;
    addr.ai_protocol = 0;
    struct addrinfo *result, *rp;
    int s, sfd;

    s = getaddrinfo(endpoint.c_str(), port.c_str(), &addr, &result);

    if (s != 0) {
        std::fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        std::exit(-1);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == nullptr) exit(-1);
    freeaddrinfo(result);
    fd = sfd;
    
    std::string req = "GET /hi/start/json HTTP/1.1\nHost: " + endpoint + "\n\n";
    write(sfd, req.c_str(), req.size());
    char buf[512];
    read(sfd, buf, 255);

    std::istringstream is(buf);
    std::string tmp, URI;
    std::getline (is, tmp);
    std::getline (is, tmp);
    std::getline (is, tmp);
    std::istringstream is02(tmp);

    is02 >> tmp >> URI;
    sessionID = URI.substr(10, 40 );
    req = "GET " + URI + "\nHost: " + endpoint + "\nConnection: Keep-Alive\n\n";
    write(sfd, req.c_str(), req.size());
    read(sfd, buf, 512);
    std::istringstream is03(buf);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp); // JSON は8行目にある 酷い……
    picojson::value v;
    is03 >> v;
    // std::cout << v;
    auto g = v.get("grid");
    picojson::array arr = g.get<picojson::array>();
    for(int i(0); i < 4; ++i){
        picojson::array a = arr[i].get<picojson::array>();
        for(int j(0); j < 4; ++j)
            grid[i][j] =  a[j].get<double>();
    }
    // for(auto e: grid){
    //     for(auto f: e)
    //         std::cout << f;
    //     std::cout << std::endl;
    // }
}

bool Board::move(Dir dir){
    std::ostringstream req;
    req << "GET /hi/state/" << sessionID << "/move/" << dirToInt(dir) << "/json\nHost: " << endpoint << "\nConnection: Keep-Alive\n\n";
    write(fd, req.str().c_str(), req.str().size());
    char buf[512];
    read(fd, buf, 512);

    std::string tmp;
    std::istringstream is03(buf);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp);
    std::getline (is03, tmp); // JSON は8行目にある 酷い……
    picojson::value v;
    is03 >> v;

    // std::cout << v << std::endl;
    auto g = v.get("grid");
    picojson::array arr = g.get<picojson::array>();
    for(int i(0); i < 4; ++i){
        picojson::array a = arr[i].get<picojson::array>();
        for(int j(0); j < 4; ++j)
            grid[i][j] =  a[j].get<double>();
    }
    // for(auto e: grid){
    //     for(auto f: e)
    //         std::cout << f;
    //     std::cout << std::endl;
    // }
    
    if(v.get("over").evaluate_as_boolean()){
        throw v; // 寿命超えるっけ？
    }
//    std::cout << v.get("moved") ;//== "true" << std::endl;
    return v.get("moved").evaluate_as_boolean();
}

void Board::show() const {
    std::cout << "+----+----+----+----+" << std::endl;
    for(auto e: grid){
        std::cout << '|' << std::setfill(' ') << std::setw(4) << std::right << int(e[0])
                  << '|' << std::setfill(' ') << std::setw(4) << std::right << int(e[1])
                  << '|' << std::setfill(' ') << std::setw(4) << std::right << int(e[2])
                  << '|' << std::setfill(' ') << std::setw(4) << std::right << int(e[3]) << '|' << std::endl;;
    }
    std::cout << "+----+----+----+----+" << std::endl;
}

int Board::toDead(std::pair<bool,Grid> const& grid) {
    std::pair<bool,Grid> movedGrid = grid;
    while(alive(movedGrid.second)){ // 死ぬまで
        auto oldGrid = movedGrid;
        int flg(0);
        int toI = rand4(mt);
        flg = 1 << (toI+1);
        auto to = allDirs[toI];
        movedGrid = movedAndBirth(oldGrid.second, to);
        if(movedGrid.first == false){
            while(flg != (1 << 4) + (1 << 3) + (1 << 2) + (1 << 1)){
                int tmp = rand4(mt);
                if(1 << tmp & flg) continue; // 既に見た
                // あった
                movedGrid = movedAndBirth(oldGrid.second, allDirs[tmp]);
                flg = 1 << (tmp+1);
                if(movedGrid.first) goto label; // goto!!
            }
            movedGrid = oldGrid;
            break; // 全部ダメだった
        }
    label: ;
    }
    return staticEval(movedGrid.second);
}

Dir Board::dicideDir() {
    auto npw = nextPossibleWorld(grid);
    decltype(npw) npw2;
    for(auto const& e: npw){
        for(auto const& e2: nextPossibleWorld(e.first))
            npw2.insert(make_pair(e2.first, e.second));
    }
    decltype(npw) npw3;
    for(auto const& e: npw2){
        for(auto const& e2: nextPossibleWorld(e.first))
            npw3.insert(make_pair(e2.first, e.second));
    }
    // decltype(npw) npw4;
    // for(auto const& e: npw3){
    //     for(auto const& e2: nextPossibleWorld(e.first))
    //         npw3.insert(make_pair(e2.first, e.second));
    // }
    return std::max_element(std::begin(npw3), std::end(npw3))->second;
    // int const MAX_ITERATION = 4000;
    // std::array<unsigned long,4> sums, counts, aves;
    // sums.fill(0);
    // counts.fill(0);
    // aves.fill(0);
    // for(int i(0); i < MAX_ITERATION; ++i){
    //     int dirI = rand4(mt);
    //     // std::cout << dirI;
    //     Dir dir = allDirs[dirI];
    //     auto movedGrid = movedAndBirth(grid, dir);
    //     // std::cout << "a";
    //     sums[dirToInt(dir)] += toDead(movedGrid);
    //     ++counts[dirToInt(dir)];
    // }
    // for(int i(0); i < 4; ++i) aves[i] = sums[i]/counts[i];
    // Dir maxDir = Dir::Down;
    // int maxAve = 0;
    // for(int i(0); i < 4; ++i){
    //     // std::cout << aves[i] << std::endl;
    //     if(aves[i] > maxAve){
    //         maxDir = allDirs[i];
    //         maxAve = aves[i];
    //     }
    // }
    // // std::cout << dirToInt(maxDir);
    // auto m = moved(grid, maxDir);
    // for(int i(0); i < 4; ++i)
    //     for(int j(0); j < 4; ++j)
    //         if(m[i][j] != grid[i][j]) {
    //             // std::cout << "!!!!!" << std::endl;
    //             // std::cout << i << ',' << j << ',' << m[i][j] << ',' << grid[i][j];
    //             return maxDir;
    //         }
    // // std::cout << "xxxxx";
    // return allDirs[rand4(mt)]; // つらい
    // return maxDir;
}

Board::Grid Board::rotate(Board::Grid const& grid, Dir dir) const{
    if(dir == Dir::Up) return grid;
    if(dir == Dir::Right)
        return {{{{grid[0][3], grid[1][3], grid[2][3], grid[3][3]}},
                {{ grid[0][2], grid[1][2], grid[2][2], grid[3][2]}},
                {{ grid[0][1], grid[1][1], grid[2][1], grid[3][1]}},
                {{ grid[0][0], grid[1][0], grid[2][0], grid[3][0]}} }};
    if(dir == Dir::Down)
        return {{{{grid[3][3], grid[3][2], grid[3][1], grid[3][0]}},
                {{ grid[2][3], grid[2][2], grid[2][1], grid[2][0]}},
                {{ grid[1][3], grid[1][2], grid[1][1], grid[1][0]}},
                {{ grid[0][3], grid[0][2], grid[0][1], grid[0][0]}} }};    if(dir == Dir::Left)
        return {{{{grid[3][0], grid[2][0], grid[1][0], grid[0][0]}},
                {{ grid[3][1], grid[2][1], grid[1][1], grid[0][1]}},
                {{ grid[3][2], grid[2][2], grid[1][2], grid[0][2]}},
                {{ grid[3][3], grid[2][3], grid[1][3], grid[0][3]}} }};
    return {{}}; // never come
}

void Board::moveUpImp(std::array<int,4>& tmp) const{
    bool joined = false;
    bool hit = false;
    for(int i(0); i < 4;++i){
        joined = false;
        if(tmp[i] == 0) continue;
        hit = false;
        for(int j(i-1); j >= 0;--j){
            if(tmp[j] == 0) continue;
            if(tmp[j] == tmp[i] && !joined){
                tmp[j] *= 2;
                tmp[i] = 0;
                joined = true;
            }else{
                if(j + 1 != i){
                    tmp[j+1] = tmp[i];
                    tmp[i] = 0;
                }
                joined = false;
            }
            hit = true;
            break;
        }
        if(i != 0 && ! hit){
            tmp[0] = tmp[i];
            tmp[i] = 0;
            joined = false;
        }
    }
}

Board::Grid Board::moveUp(Board::Grid const& grid) const{
    Board::Grid newGrid;
    for(int i(0); i < 4;++i)
        for(int j(0); j < 4;++j)
            newGrid[i][j]=0;
    for(int i(0); i < 4;++i){
        std::array<int,4> tmp;
        for(int j(0); j < 4;++j){
            tmp[j] = grid[j][i];
        }
        moveUpImp(tmp);
        for(int j(0); j < 4;++j){
            newGrid[j][i] = tmp[j];
        }
    }
    return newGrid;
}
Board::Grid Board::moved(Board::Grid const& grid, Dir dir) const{
    if(dir != Dir::Up) return rotate((moveUp (rotate(grid, mirror(dir)))), dir);

    return moveUp(grid);
}

bool Board::movable(Board::Grid const& grid, Dir dir) const{
    return moved(grid, dir) != grid;
}

std::pair<bool,Board::Grid> Board::movedAndBirth(Board::Grid const& grid, Dir dir) {
    auto m = moved(grid, dir);
    if(m == grid) return std::make_pair(false, m);
    int birth = "2222222224"[rand10(mt)] - '0';
    int zeros(0);
    for(int i(0); i < 4; ++i)
        for(int j(0); j < 4; ++j)
            if(m[i][j] == 0) ++zeros;
    std::uniform_int_distribution<int> dice(0,zeros-1);
    int pos = dice(mt);
    for(int i(0); i < 4; ++i)
        for(int j(0); j < 4; ++j)
            if(m[i][j] == 0)
                if(pos-- == 0) m[i][j] = birth;
    return std::make_pair(true, m);
}

bool Board::alive(Board::Grid const& grid) const{
    for(int i(0); i < 4;++i) if(movable(grid, allDirs[i])) return true;
    return false;
}
double Board::staticEval(Board::Grid const& grid){
    double sum(0);
    for(int i(0); i < 4; ++i)
        for(int j(0); j < 4; ++j)
            sum += grid[i][j] * log2(grid[i][j]);
    return sum;
}

int Board::log2(int i){
    switch(i){
    case 0:
        return 0;
    case 2:
        return 1;
    case 4:
        return 2;
    case 8:
        return 3;
    case 16:
        return 4;
    case 32:
        return 5;
    case 64:
        return 6;
    case 128:
        return 7;
    case 256:
        return 8;
    case 512:
        return 9;
    case 1024:
        return 10;
    case 2048:
        return 11;
    case 4096:
        return 12;
    case 8192:
        return 13;
    case 16384:
        return 14;
    default:
        return 15;
    }
}

Board::GridList_t<std::pair<Board::Grid, Dir>> Board::nextPossibleWorld(Board::Grid const& grid) const{
    GridList_t<std::pair<Grid, Dir>> vec;
    for(auto dir: allDirs){
        GridList_t<std::pair<Grid, Dir>> tmp;
        tmp = nextPossibleWorldUp(rotate(grid, dir), dir);
        for(auto const& e: tmp) vec.insert(std::make_pair(rotate(e.first, mirror(dir)), dir));
    }
    return vec;
}

Board::GridList_t<std::pair<Board::Grid, Dir>> Board::nextPossibleWorldUp(Board::Grid const& grid, Dir dir) const{
    GridList_t<std::pair<Grid, Dir>> ups;
    auto up = moveUp(grid);
    if(up == grid) return {};
    // ups.reserve(16);
    for(int i(0); i < 4; ++i)
        for(int j(0); j < 4; ++j)
            if(up[j][i] == 0){
                Grid tmp(up), tmp2(up);
                for(int i(0); i < 4;++i)
                    for(int j(0); j < 4;++j)
                        tmp[i][j] = tmp2[i][j] = up[i][j];
                tmp[j][i] = 2;
                tmp2[j][i] = 4;
                ups.insert(std::make_pair(tmp, dir));
                ups.insert(std::make_pair(tmp2, dir));
                break;
            }
    return ups;
}

