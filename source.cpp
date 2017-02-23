#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <boost/xpressive/xpressive.hpp>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <oauth.h>
#include <time.h>

#include <linux/input.h>

#define cerr(str) std::cerr<<"\x1b[41m"<<str<<"\x1b[40m"<<std::endl

const std::string input_path_stem = "/dev/input/event";
const std::string config_file = "main.conf";

enum WATCH_MODE{
	WATCH_MODE_RUN,
	WATCH_MODE_TEST
};

std::string get_event_file_name(std::string input_device_name){
	int fd, c = 0;
	char device_name[256] = "Unknown";
	char *device = NULL;
	//デバイス探索
	while( (fd = open((input_path_stem+std::to_string(c)).c_str(), O_RDONLY)) != -1 ){
		ioctl( fd, EVIOCGNAME(sizeof(device_name)),device_name	);
		if(input_device_name.compare(device_name) == 0){
			return input_path_stem + std::to_string(c);
		}
		c++;
	}
	return "";
}

int load_config(std::map<std::string,std::string>& config_map){
	using namespace boost::xpressive;
	config_map.clear();
	std::ifstream ifs(config_file);
	if(ifs){
		std::string line;
		mark_tag key(1), content(2);
		sregex r0 = bos>>*_s>>(key=+_w)>>*_s>>"=">>*_s>>as_xpr('\"')>>(content=+~_n)>>as_xpr("\"")>>*_s>>eos;
		sregex r1 = bos>>*_s>>(key=+_w)>>*_s>>"=">>*_s>>(content=+_w)>>*_s;
		smatch what;
		while( std::getline(ifs,line) ){
			if(regex_match(line,what,r0) || regex_match(line,what,r1)){
				config_map.insert(std::make_pair(what[key],what[content]));
			}
		}
		// 設定のバリデーション
		std::vector<std::string> required_key{"consumer_key","consumer_secret","access_token","access_token_secret","input_device_name"};
		int error = 0;
		for(auto key : required_key ){
			if( config_map.find(key) == config_map.end() ){ 
				cerr(key+"が設定されていません.");
				error = 1;
			}
		}
		if( error ){
			return 1;
		}
		std::string event_file_name = get_event_file_name(config_map["input_device_name"]);
		if(event_file_name.length() == 0){
			cerr("デバイス"<<config_map["input_device_name"]<<"は存在しません.");
			return 1;
		}else{
			config_map.insert(std::make_pair("event_file_name",event_file_name));
		}
	}else{
		cerr(config_file<<"を開けません");
	}
	return 0;
}

void show_devices(){
	int fd, c = 0;
	char device_name[256] = "Unknown";
	char *device = NULL;
	//デバイス探索
	while( (fd = open((input_path_stem+std::to_string(c)).c_str(), O_RDONLY)) != -1 ){
		ioctl( fd, EVIOCGNAME(sizeof(device_name)),device_name	);
		std::cout<<c++<<" : "<<device_name<<std::endl;
	}
}


int tweet(std::string text,std::string consumer_key,std::string consumer_secret,std::string access_token,std::string access_token_secret){
	CURL* curl = curl_easy_init();
	if( !curl ){
		return 1;
	}
	std::string url = "https://api.twitter.com/1.1/statuses/update.json";
	curl_slist* slist = NULL;
	char **argv,*auth_params,*non_auth_params,*tmp_url;
	int argc;
	argv = (char**)malloc(0);
	std::string escaped_args = oauth_url_escape(("status="+text).c_str());
	std::string ser_url = url + "?" + escaped_args ;

	argc = oauth_split_url_parameters(ser_url.c_str(),&argv);
	tmp_url = oauth_sign_array2(
			&argc,
			&argv,
			NULL,
			OA_HMAC,
			"POST",
			consumer_key.c_str(),
			consumer_secret.c_str(),
			access_token.c_str(),
			access_token_secret.c_str()
			);
	if(tmp_url == NULL){
		return 1;
	}
	free(tmp_url);
	// create request header
	auth_params = oauth_serialize_url_sep(
			argc,
			1,
			argv,
			(char*)", ",
			6
			);
	std::string auth_header = std::string("Authorization: OAuth ") + auth_params;
	free(auth_params);
	slist = curl_slist_append(slist, auth_header.c_str() );

	non_auth_params = oauth_serialize_url_sep(
			argc,
			1,
			argv,
			(char*)"&",
			1
			);
	curl_easy_setopt(curl,CURLOPT_POSTFIELDS,non_auth_params);
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,slist);
	curl_easy_setopt(curl,CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl,CURLOPT_POST,1);
	curl_easy_setopt(curl,CURLOPT_USE_SSL, CURLUSESSL_ALL);
	for(int i = 0;i < argc;i++){
		free(argv[i]);
	}
	free(argv);

	CURLcode status = curl_easy_perform(curl);
	if( status ){
		return 1;
	}
	return 0;
}
std::string get_date(time_t timer = 0){
	struct tm *date;
	char str[256];
	if(timer == 0)
		timer = time(NULL);
	date = localtime(&timer);  
	strftime(str, 255, "%Y/%m/%d %H時%M分%S秒", date);
	return std::string(str);
}


int watch(WATCH_MODE mode,std::map<std::string,std::string> &config_map){
	int fd = open( config_map["event_file_name"].c_str() , O_RDONLY );
	if( fd == -1 )return 1;
	struct input_event event;
	time_t prev_time = time(NULL);
	time_t interval = 2;
	while(1){
		if( read( fd , &event , sizeof( input_event ) ) < sizeof( input_event ) ){
			cerr("キー情報を取得失敗");
			close( fd );
			return 1;
		}
		std::string key=std::to_string(event.type)+"_"+std::to_string(event.code);
		if(mode == WATCH_MODE_RUN && config_map.find(key) != config_map.end() ){
			std::string tweet_string = config_map[key] + "\n\n["+get_date()+"]";
			if(prev_time + interval < time(NULL)){
				if(tweet(tweet_string,config_map["consumer_key"],config_map["consumer_secret"],config_map["access_token"],config_map["access_token_secret"]) == 1){
					cerr("ツイート失敗");
				}
				prev_time = time(NULL);
			}
		}else if( mode == WATCH_MODE_TEST ){
			std::cout<<"key="<<key<<std::endl;
		}
	}
	close( fd );
}

int main(int argc,char** argv){
	//ルートでの起動を確認
	if( getuid() != 0 ){
		cerr("rootで実行してください");
		return 1;
	}
	//引数解析
	if(argc == 2){
		std::string option = argv[1];
		if( option.compare("devices") == 0 ){
			show_devices();
		}else if( option.compare("start") == 0 ){
			std::map<std::string,std::string> config_map;
			if(load_config( config_map ) == 0){
				return watch(WATCH_MODE_RUN, config_map );
			}else{
				return 1;
			}
		}else if( option.compare("keytest") == 0 ){
			std::map<std::string,std::string> config_map;
			if(load_config( config_map ) == 0){
				return watch( WATCH_MODE_TEST, config_map );
			}else{
				return 1;
			}
		}else{
			cerr("不正な引数です");
		}
	}else{
		cerr("引数の個数が異常です");
	}
}
