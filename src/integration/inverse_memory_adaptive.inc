  // MXIM3 is emitted only by the explicitly unbounded adaptive profile.
  // Historical bounded MXIM1/MXIM2 snapshots retain their exact bytes.
  std::string adaptive_snapshot() const {
    const auto hex_bytes=[](const std::string& bytes){
      constexpr char digits[]="0123456789abcdef";
      std::string out;out.reserve(bytes.size()*2);
      for(unsigned char c:bytes){out.push_back(digits[c>>4]);out.push_back(digits[c&15]);}
      return out;
    };
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"MXIM3 "<<unsigned(background_)<<'\n';
    overrides_->for_each_page_record([&](std::uint64_t page,
                                           const std::string& bitmap,
                                           const std::string& values){
      out<<"P "<<page<<' '<<hex_bytes(bitmap)<<' '<<hex_bytes(values)<<'\n';
    });
    overrides_->for_each_large_address([&](const auto& entry){
      out<<"O "<<entry.first<<' '<<unsigned(entry.second)<<'\n';
    });
    bindings_->for_each_sorted([&](const auto& entry){
      out<<"Q "<<entry.first<<' '<<entry.second<<'\n';
    });
    return out.str();
  }
  // C43: the very same MXIM2/MXIM3 bytes, emitted one page at a time.
  // Keep adaptive_snapshot() as the independent legacy byte oracle.
  template<class Emit> void persistent_snapshot_to(Emit&& emit) const {
    if(limits_.max_overrides){const auto saved=persistent_snapshot();emit(saved.data(),saved.size());return;}
    auto text=[&](const std::string& value){emit(value.data(),value.size());};
    auto hex=[](const std::string& bytes){
      constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(bytes.size()*2);
      for(unsigned char value:bytes){result.push_back(digits[value>>4]);result.push_back(digits[value&15]);}
      return result;
    };
    text("MXIM2 "+std::to_string(revision_)+"\nMXIM3 "+std::to_string(unsigned(background_))+"\n");
    overrides_->for_each_page_record([&](std::uint64_t page,const std::string& bitmap,const std::string& values){
      text("P "+std::to_string(page)+" "+hex(bitmap)+" "+hex(values)+"\n");
    });
    overrides_->for_each_large_address([&](const auto& item){text("O "+item.first+" "+std::to_string(unsigned(item.second))+"\n");});
    bindings_->for_each_sorted([&](const auto& item){text("Q "+item.first+" "+item.second+"\n");});
  }
  std::size_t persistent_snapshot_size() const {
    if(limits_.max_overrides)return persistent_snapshot().size();
    std::size_t size=std::string("MXIM2 "+std::to_string(revision_)+"\nMXIM3 "+std::to_string(unsigned(background_))+"\n").size();
    auto add=[&](std::size_t count){if(count>std::numeric_limits<std::size_t>::max()-size)throw std::length_error("Ether snapshot size overflow");size+=count;};
    overrides_->for_each_page_extent([&](std::uint64_t page,std::size_t values){
      add(5+std::to_string(page).size()+1024+values*2);
    });
    overrides_->for_each_large_address([&](const auto& item){add(4+item.first.size()+std::to_string(unsigned(item.second)).size());});
    bindings_->for_each_sorted([&](const auto& item){add(4+item.first.size()+item.second.size());});
    return size;
  }
  // Read-only host transport. Every replacement retains its original MXIM
  // spelling; deletion removes that record from the reconstructed RVG.
  template<class Emit>void checkpoint_patch_since(const InverseMemory& previous,Emit&& emit)const{
    if(limits_.max_overrides||previous.limits_.max_overrides){
      if(limits_.max_overrides!=previous.limits_.max_overrides)throw std::invalid_argument("Checkpoint memory profile changed");
      const auto text=persistent_snapshot();if(text!=previous.persistent_snapshot())emit("B",text,false);return;
    }
    auto prefix=[](const InverseMemory& m){return "MXIM2 "+std::to_string(m.revision_)+"\nMXIM3 "+std::to_string(unsigned(m.background_))+"\n";};
    const auto header=prefix(*this);if(header!=prefix(previous))emit("M",header,false);
    auto hex=[](const std::string& bytes){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);
      for(unsigned char byte:bytes){out.push_back(digits[byte>>4]);out.push_back(digits[byte&15]);}return out;};
    overrides_->for_each_changed_page_record(*previous.overrides_,[&](std::uint64_t page,const std::string& bitmap,const std::string& values,bool removed){
      const auto id=std::to_string(page);emit("P"+id,removed?std::string{}:"P "+id+" "+hex(bitmap)+" "+hex(values)+"\n",removed);
    });
    overrides_->for_each_large_address([&](const auto& entry){const auto* old=previous.overrides_->get(entry.first);
      if(!old||*old!=entry.second)emit("O"+entry.first,"O "+entry.first+" "+std::to_string(unsigned(entry.second))+"\n",false);});
    previous.overrides_->for_each_large_address([&](const auto& entry){if(!overrides_->get(entry.first))emit("O"+entry.first,std::string{},true);});
    bindings_->for_each_sorted([&](const auto& entry){const auto* old=previous.bindings_->get(entry.first);
      if(!old||*old!=entry.second)emit("Q"+entry.first,"Q "+entry.first+" "+entry.second+"\n",false);});
    previous.bindings_->for_each_sorted([&](const auto& entry){if(!bindings_->get(entry.first))emit("Q"+entry.first,std::string{},true);});
  }
  static InverseMemory from_adaptive_snapshot(const std::string& saved,Limits limits){
    const auto unhex=[](const std::string& hex,std::size_t maximum){
      if((hex.size()&1)||hex.size()>maximum*2)
        throw std::invalid_argument("Invalid adaptive Ether hex extent");
      auto nibble=[](char ch)->unsigned{
        if(ch>='0'&&ch<='9')return unsigned(ch-'0');
        if(ch>='a'&&ch<='f')return unsigned(ch-'a'+10);
        throw std::invalid_argument("Invalid adaptive Ether hex digit");
      };
      std::string bytes;bytes.reserve(hex.size()/2);
      for(std::size_t i=0;i<hex.size();i+=2)
        bytes.push_back(char((nibble(hex[i])<<4)|nibble(hex[i+1])));
      return bytes;
    };
    std::istringstream input(saved);input.imbue(std::locale::classic());
    std::string magic,background;
    if(!(input>>magic>>background)||magic!="MXIM3")
      throw std::invalid_argument("Malformed adaptive Ether snapshot");
    InverseMemory result(byte_value(background,"Inverse Memory background"),limits);
    std::string kind,address,payload,extra;
    while(input>>kind>>address>>payload){
      if(kind=="P"){
        if(!(input>>extra))throw std::invalid_argument("Truncated adaptive Ether page");
        std::uint64_t page=0;
        const auto parsed=std::from_chars(address.data(),address.data()+address.size(),page);
        if(parsed.ec!=std::errc()||parsed.ptr!=address.data()+address.size()||
           (address.size()>1&&address[0]=='0'))
          throw std::invalid_argument("Invalid adaptive Ether page number");
        result.overrides_->load_page_record(page,unhex(payload,512),unhex(extra,4096));
        if(limits.max_overrides&&result.overrides_->size()>limits.max_overrides)
          throw std::length_error("Adaptive Ether snapshot override budget exceeded");
      }else if(kind=="O"){
        const auto canonical=result.canonical(address);
        if(!result.overrides_->insert(canonical,byte_value(payload,"Inverse Memory byte")))
          throw std::invalid_argument("Duplicate adaptive Ether override");
        if(limits.max_overrides&&result.overrides_->size()>limits.max_overrides)
          throw std::length_error("Adaptive Ether snapshot override budget exceeded");
      }else if(kind=="Q"){
        const auto canonical=result.canonical(address);
        const auto base=infinite_detail::canonical_address(payload,limits.max_address_digits);
        (void)add_small(base,7,limits.max_address_digits);
        if(result.bindings_->size()==limits.max_quantum_bindings)
          throw std::length_error("Adaptive Ether snapshot quantum binding budget exceeded");
        if(!result.bindings_->insert(canonical,base))
          throw std::invalid_argument("Duplicate adaptive Ether quantum binding");
      }else throw std::invalid_argument("Unknown adaptive Ether snapshot record");
    }
    if(!input.eof())throw std::invalid_argument("Malformed adaptive Ether record stream");
    return result;
  }

  // Same grammar and authority, without a complete memory-text copy.
  static InverseMemory from_snapshot_view(std::string_view saved,Limits limits){
    if(saved.compare(0,6,"MXIM2 ")==0){
      const auto end=saved.find('\n');
      if(end==std::string_view::npos||end<7||end>27)throw std::invalid_argument("memory revision envelope");
      std::uint64_t revision=0;
      for(std::size_t i=6;i<end;++i){const auto c=saved[i];
        if(c<'0'||c>'9'||revision>(std::numeric_limits<std::uint64_t>::max()-unsigned(c-'0'))/10)
          throw std::invalid_argument("memory revision range");
        revision=revision*10+unsigned(c-'0');
      }
      auto result=from_snapshot_view(saved.substr(end+1),limits);result.revision_=revision;return result;
    }
    if(saved.compare(0,6,"MXIM3 ")==0)return from_adaptive_snapshot_view(saved,limits);
    // Historical bounded MXIM1 input is small and retains its original decoder.
    return from_snapshot(std::string(saved),limits);
  }
  static InverseMemory from_adaptive_snapshot_view(std::string_view saved,Limits limits){
    const auto unhex=[](const std::string& hex,std::size_t maximum){
      if((hex.size()&1)||hex.size()>maximum*2)
        throw std::invalid_argument("Invalid adaptive Ether hex extent");
      auto nibble=[](char ch)->unsigned{
        if(ch>='0'&&ch<='9')return unsigned(ch-'0');
        if(ch>='a'&&ch<='f')return unsigned(ch-'a'+10);
        throw std::invalid_argument("Invalid adaptive Ether hex digit");
      };
      std::string bytes;bytes.reserve(hex.size()/2);
      for(std::size_t i=0;i<hex.size();i+=2)
        bytes.push_back(char((nibble(hex[i])<<4)|nibble(hex[i+1])));
      return bytes;
    };
    memory_detail::SnapshotViewStream input(saved);input.imbue(std::locale::classic());
    std::string magic,background;
    if(!(input>>magic>>background)||magic!="MXIM3")
      throw std::invalid_argument("Malformed adaptive Ether snapshot");
    InverseMemory result(byte_value(background,"Inverse Memory background"),limits);
    std::string kind,address,payload,extra;
    while(input>>kind>>address>>payload){
      if(kind=="P"){
        if(!(input>>extra))throw std::invalid_argument("Truncated adaptive Ether page");
        std::uint64_t page=0;
        const auto parsed=std::from_chars(address.data(),address.data()+address.size(),page);
        if(parsed.ec!=std::errc()||parsed.ptr!=address.data()+address.size()||
           (address.size()>1&&address[0]=='0'))
          throw std::invalid_argument("Invalid adaptive Ether page number");
        result.overrides_->load_page_record(page,unhex(payload,512),unhex(extra,4096));
        if(limits.max_overrides&&result.overrides_->size()>limits.max_overrides)
          throw std::length_error("Adaptive Ether snapshot override budget exceeded");
      }else if(kind=="O"){
        const auto canonical=result.canonical(address);
        if(!result.overrides_->insert(canonical,byte_value(payload,"Inverse Memory byte")))
          throw std::invalid_argument("Duplicate adaptive Ether override");
        if(limits.max_overrides&&result.overrides_->size()>limits.max_overrides)
          throw std::length_error("Adaptive Ether snapshot override budget exceeded");
      }else if(kind=="Q"){
        const auto canonical=result.canonical(address);
        const auto base=infinite_detail::canonical_address(payload,limits.max_address_digits);
        (void)add_small(base,7,limits.max_address_digits);
        if(result.bindings_->size()==limits.max_quantum_bindings)
          throw std::length_error("Adaptive Ether snapshot quantum binding budget exceeded");
        if(!result.bindings_->insert(canonical,base))
          throw std::invalid_argument("Duplicate adaptive Ether quantum binding");
      }else throw std::invalid_argument("Unknown adaptive Ether snapshot record");
    }
    if(!input.eof())throw std::invalid_argument("Malformed adaptive Ether record stream");
    return result;
  }
