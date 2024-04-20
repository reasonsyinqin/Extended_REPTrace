# coding=utf-8
"""
import /tmp/traceData.dat into table trace_info
"""
#import MySQLdb
import sys
reload(sys)  
sys.setdefaultencoding('utf8')

#import MySQLdb
import os
from treelib import Tree, Node
from warnings import filterwarnings
#filterwarnings('error', category = MySQLdb.Warning)
# key=pid,value=pname
pname_dict = {}
pname_string = '''root      3036  9.2  0.1 441968 104056 ?       Ss   16:59   0:03 /usr/bin/python2 /usr/bin/nova-conductor
root      3204  4.1  0.1 452744 110416 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3207  4.3  0.1 451912 109612 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3208  4.6  0.1 454768 112392 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3209  4.8  0.1 454720 112308 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3212  4.3  0.1 454708 112412 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3213  4.8  0.1 453140 110680 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3214  4.7  0.1 452828 110556 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3215  4.4  0.1 451864 109664 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3218  4.5  0.1 452288 109932 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3221  4.9  0.1 452260 109944 ?       R    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3223  4.8  0.1 452120 109684 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3224  4.9  0.1 452252 109888 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3226  5.2  0.1 453672 111312 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3228  5.0  0.1 453964 111696 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3231  5.1  0.1 453856 111476 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3232  4.3  0.1 454564 112144 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3233  4.6  0.1 451948 109652 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3235  5.0  0.1 456344 114060 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3237  4.0  0.1 452120 109636 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3238  3.7  0.1 451760 109508 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3240  4.1  0.1 451864 109460 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3241  3.0  0.1 451768 109572 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3243  2.6  0.1 452192 109848 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-conductor
root      3245  3.0  0.1 454636 112232 ?       S    16:59   0:01 /usr/bin/python2 /usr/bin/nova-conductor
root      3274  9.7  0.1 449380 111360 ?       Rs   16:59   0:03 /usr/bin/python2 /usr/bin/nova-scheduler
root      3331  2.1  0.1 452604 110144 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3332  1.2  0.1 450828 108528 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3333  1.2  0.1 450828 108492 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3334  1.1  0.1 450828 108528 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3335  1.2  0.1 450852 108532 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3337  1.2  0.1 450832 108492 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3340  1.1  0.1 450844 108484 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3341  1.1  0.1 450832 108496 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3345  1.0  0.1 450780 108504 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3347  1.0  0.1 450824 108484 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3376 15.0  0.1 2113368 148108 ?      Ssl  16:59   0:04 /usr/bin/python2 /usr/bin/nova-compute
root      3380  0.4  0.1 450804 108460 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      3398  0.5  0.1 450812 108468 ?       S    16:59   0:00 /usr/bin/python2 /usr/bin/nova-scheduler
root      1678 11.6  0.1 15407224 138816 pts/7 Sl+  16:58   0:06 /usr/lib64/rabbitmq'''


class event:
    def __init__(self, event_id=None,call_id=None,ftype_str=None,on_ip=None,on_port=None,in_ip=None,in_port=None,process_id=None,thread_id=None,p_process_id=None,pname=None,parent_event_id=None,
    MSG_ID=None,MSG_CTX_ID=None,time_info=None,rtype=None,http_response_status_code=None):
        self.event_id=event_id
        self.call_id=call_id #ftype
        self.ftype_str=ftype_str
        self.on_ip=on_ip
        self.on_port=on_port
        self.in_ip=in_ip
        self.in_port=in_port
        self.process_id=process_id #pid
        self.thread_id=thread_id #ktid
        self.p_process_id=p_process_id #ppid
        self.pname=pname 
        self.parent_event_id=parent_event_id
        self.MSG_ID=MSG_ID # uuid data_id
        self.MSG_CTX_ID=MSG_CTX_ID # unit_uuid-unit_data_id
        self.time_info=time_info #ttime ltime both in datafile
        self.rtype=rtype
        self.http_response_status_code=http_response_status_code

    def set_rtype(self,rtype):
        self.rtype=rtype
    def get_rtype(self):
        return self.rtype
    def set_event_id(self,event_id):
        self.event_id=event_id
    def get_event_id(self):
        return self.event_id
    def set_MSG_ID(self,MSG_ID):
        self.MSG_ID=MSG_ID
    def get_MSG_ID(self):
        return self.MSG_ID
    def set_parent_event_id(self,parent_event_id):
        self.parent_event_id=parent_event_id
    def get_parent_event_id(self):
        return self.parent_event_id
    def set_in_ip(self,in_ip):
        self.in_ip=in_ip
    def get_in_ip(self):
        return self.in_ip
    def set_call_id(self,call_id):
        self.call_id=call_id
    def get_call_id(self):
        return self.call_id
    def set_thread_id(self,thread_id):
        self.thread_id=thread_id
    def get_thread_id(self):
        return self.thread_id
    def set_p_process_id(self,p_process_id):
        self.p_process_id=p_process_id
    def get_p_process_id(self):
        return self.p_process_id
    def set_process_id(self,process_id):
        self.process_id=process_id
    def get_process_id(self):
        return self.process_id
    def set_on_ip(self,on_ip):
        self.on_ip=on_ip
    def get_on_ip(self):
        return self.on_ip
    def set_MSG_CTX_ID(self,MSG_CTX_ID):
        self.MSG_CTX_ID=MSG_CTX_ID
    def get_MSG_CTX_ID(self):
        return self.MSG_CTX_ID
    def set_time_info(self,time_info):
        self.time_info=time_info
    def get_time_info(self):
        return self.time_info
    def set_on_port(self,on_port):
        self.on_port=on_port
    def get_on_port(self):
        return self.on_port
    def set_pname(self,pname):
        self.pname=pname
    def get_pname(self):
        return self.pname
    def set_in_port(self,in_port):
        self.in_port=in_port
    def get_in_port(self):
        return self.in_port

    def show(self):
        print ('event_id='+str(self.event_id)+',call_id='+str(self.call_id)+',ftype_str='+str(self.ftype_str)+',on_ip='+str(self.on_ip)+',on_port='+str(self.on_port)+',in_ip='+str(self.in_ip)
        +',in_port='+str(self.in_port)+',process_id='+str(self.process_id)+',thread_id='+str(self.thread_id)+',p_process_id='+str(self.p_process_id)+',pname='+str(self.pname)
        +',parent_event_id='+str(self.parent_event_id)+',MSG_ID='+str(self.MSG_ID)+',MSG_CTX_ID='+str(self.MSG_CTX_ID)+',time_info='+str(self.time_info)+',rtype='+str(self.rtype))

def convertDataToDict(line):
    sqlDict = {}
    attr = line.strip().split("&")
    for item in attr:
        pair = item.strip().split("=",1)
        if(len(pair)==2):
            sqlDict[pair[0]] = pair[1]
        else:
            sqlDict['rtype']=1
    return sqlDict

def getParent(e,S):
    # print ('now we find'+str(e.event_id)+': parent')
    if e.call_id=='2' and e.MSG_ID is not None and e.MSG_ID != '': # Type 3
        #e.show()
        for p in S:
            if e.event_id==p.event_id:
                continue
            elif e.MSG_ID==p.MSG_ID:
               # print 'the'
               # p.show()
               # print 'is parent of'
               # e.show() 
                return p
    else:
        #if e.MSG_ID is None or e.MSG_ID == '':
         #   return None
            # Type 1
        closest_event = event(time_info=999999999999999999)
        i = 0
        for p in S:
            #print i+1 , ':'
            i = i+1
            if e.event_id==p.event_id:
                continue
            #elif e.MSG_CTX_ID==p.MSG_CTX_ID and e.thread_id == p.thread_id:
            elif e.process_id == p.process_id:
                #print 'the e',e.time_info,e.event_id,e.thread_id
                #print 'the p',p.time_info,p.event_id,p.thread_id
                #print 'the closest_event',closest_event.time_info,closest_event.event_id,closest_event.thread_id
                # 用p遍历S,发现e发生时间在p之后，就比较p和c，把c更新为p和c里面更大的（更靠后，更接近e的）
                if e.time_info > p.time_info:
                    #print 'p:'
                    #p.show()
                    #print 'the e',e.time_info,e.event_id,e.thread_id
                    #print 'the p',p.time_info,p.event_id,p.thread_id
                    #print 'the closest_event',closest_event.time_info,closest_event.event_id,closest_event.thread_id
                    #print (closest_event.time_info + int(p.time_info))
                    if closest_event.time_info > int(p.time_info):
                        closest_event = p
                        # print 'the closest_event',closest_event.time_info,closest_event.event_id,closest_event.thread_id
                        # closest_event.show()
        #if closest_event is not None:
            #print ('the','the closest_event',closest_event.time_info,closest_event.event_id,closest_event.thread_id
            #,'is parent of'
            #,'the e',e.time_info,e.event_id,e.thread_id)
        #else:
            #print ('the'+str(e.event_id)+' has no parent')
        return closest_event

def gen_pname_dict():
    pname_lines = pname_string.split('\n')
    for pname_line in pname_lines:
        #print "pname_line=",pname_line
        pname_line0 = pname_line.split( )
        pid = pname_line0[1]
        #print pname_line0
        pname_list = pname_line0[-1].split('/')
        #print pname_list
        p_name_helper1 = pname_list[-1].split('.')
        #print p_name_helper1
        p_name = p_name_helper1[0]
        #print pid
        #print p_name
        pname_dict[pid]=p_name

def get_pname(pid):
    p_name = ''
    #print 'ps -eLf | grep %s' % str(pid)
    if pname_dict.has_key(pid):
        return pname_dict[pid]
    else:
        return ""
        with os.popen('ps -eLf | grep %s' % str(pid)) as p:
            pname_string = p.read()
            pname_lines = pname_string.split('\n')
                #print pname_lines[0]
            pname_line0 = pname_lines[0].split( )
                #print pname_line0[10]
            pname_list = pname_line0[10].split('/')
                #print pname_list
            if len(pname_list) >= 4:
                        # print pname_list_row
                p_name_helper1 = pname_list[3].split('.')
                        #print p_name_helper1
                p_name = p_name_helper1[0]
        return p_name

def covFTypeToStr(f_type):
    f_type_str = ''
    if f_type=='1':
        f_type_str = 'send'
    elif f_type=='2':
        f_type_str = 'recv'
    elif f_type=='3':
        f_type_str = 'write'
    elif f_type=='4':
        f_type_str = 'read'
    elif f_type=='5':
        f_type_str = 'sendmsg'
    elif f_type=='6':
        f_type_str = 'recvmsg'
    elif f_type=='7':
        f_type_str = 'sendto'
    elif f_type=='8':
        f_type_str = 'recvfrom'
    elif f_type=='9':
        f_type_str = 'writev'
    elif f_type=='10':
        f_type_str = 'readv'
    elif f_type=='11':
        f_type_str = 'sendmmsg'
    elif f_type=='12':
        f_type_str = 'recvmmsg'
    elif f_type=='13':
        f_type_str = 'connect'
    elif f_type=='14':
        f_type_str = 'socket'
    elif f_type=='15':
        f_type_str = 'close'
    elif f_type=='17':
        f_type_str = 'send64'
    elif f_type=='19':
        f_type_str = 'sendfile'
    elif f_type=='20':
        f_type_str = 'accept'
    elif f_type=='21':
        f_type_str = 'fork'
    elif f_type=='22':
        f_type_str = 'vfork'
    elif f_type=='23':
        f_type_str = 'PTCreate'
    elif f_type=='23':
        f_type_str = 'PTJoin'
    else:
        f_type_str = f_type
    return f_type_str

def getTraces(filename):
    dataFile = open(filename, 'r')
    traceData = []
    dictKey = ['ftype','on_ip','on_port','in_ip','in_port','pid','ktid','ppid','uuid','unit_uuid','ttime','ltime','rtype','http_response_status_code']
    for line in dataFile:
        lineDict = convertDataToDict(line)
        lineValue = []
        for key in dictKey:
            if key in lineDict:
                lineValue.append(lineDict[key])
            else:
                lineValue.append(None)
        # print lineValue

        #ttime ltime both in datafile
        if lineValue[10]==None:
            time_info=lineValue[11]
        else:
            time_info=lineValue[10]
        traceData.append(event(call_id=lineValue[0],on_ip=lineValue[1],on_port=lineValue[2],in_ip=lineValue[3],in_port=lineValue[4],process_id=lineValue[5],thread_id=lineValue[6],
        p_process_id=lineValue[7],MSG_ID=lineValue[8],MSG_CTX_ID=lineValue[9],time_info=time_info,rtype=lineValue[12],http_response_status_code=lineValue[13]))
    # 虽然是按照时间添加的trace（默认有序），但是为了保险还是排一次
    traceData_sorted = sorted(traceData,key = lambda x:x.time_info)
    for i in range(len(traceData_sorted)):
        # 添加event_id
        traceData_sorted[i].event_id = i+1
        # 处理两个ftype
        # is thread
        if traceData_sorted[i].rtype == '27':
            if traceData_sorted[i].p_process_id==traceData_sorted[i].process_id:
                traceData_sorted[i].call_id = '23'
                #traceData_sorted[i].set_call_id(23)
            else:
                traceData_sorted[i].call_id = '21'
                #traceData_sorted[i].set_call_id(21)
            #traceData_sorted[i].show()
    # 添加parent_event_id，pname，ftype_str
    for row in traceData_sorted:
        #row.show()
        p = getParent(row,traceData_sorted)
        if p is not None:
            row.parent_event_id = p.event_id
        p_name = get_pname(row.process_id)
        
        if p_name is not None:
            row.pname = p_name
        row.ftype_str = covFTypeToStr(row.call_id)
    #for row in traceData_sorted:
        #print row.event_id,row.parent_event_id
        #row.show()
    return traceData_sorted
    #print '**********'
# row 就是class event
def covTraceToForest(traces):
    forest = []
    
    # 先把parent_event_id为None的处理了 作为根节点
    for row in traces:
        parent_event_id = row.parent_event_id
        if parent_event_id == None:
            traceTree = Tree()

            traceTree.create_node(tag=row.event_id,identifier=row.event_id,parent=row.parent_event_id,data=row)
            forest.append(traceTree)

    # 先暂时假设这个树的深度不超过100
    for i in range(100):
        for row in traces:
            # 先判断这个节点的父亲是不是在森林中，如果不在，就暂时跳过
            for tree in forest:
                parent_event_id = row.parent_event_id
                if tree.contains(parent_event_id):
                    #在就直接添加
                    is_parent_in_forest = True
                    if not tree.contains(row.event_id):
                        #print "event_id=%s,parent_event_id=%s" % \
                            #(row.event_id, row.parent_event_id )
                        tree.create_node(tag=row.event_id,identifier=row.event_id,parent=row.parent_event_id,data=row)
                        #print 'I add a node to '
                    break
            
    return forest

def textSave(filename, data):#filename为写入CSV文件的路径，data为要写入数据列表.
  file = open(filename,'a')
  for i in range(len(data)):
    #s = str(data[i]).replace('[','').replace(']','')#去除[],这两行按数据不同，可以选择
    #s = s.replace("'",'').replace(',','') +'\n'  #去除单引号，逗号，每行末尾追加换行符
    file.write(data[i])
  file.close()

def mkdir(path):
 
	folder = os.path.exists(path)
 
	if not folder:                   #判断是否存在文件夹如果不存在则创建为文件夹
		os.makedirs(path)            #makedirs 创建文件时如果路径不存在会创建这个路径
		#print "---  new folder...  ---"
		#print "---  OK  ---"
 
	#else:
		#print "---  There is this folder!  ---"

def filterTrace(forest):
    new_forest = []
    for tree in forest:
        #tree.show()
        nodelist = tree.all_nodes()
        for node in nodelist:
            #print 'node',node.data.event_id,'data.call_id=',node.data.call_id
            if node.is_root():
                root_event_id = node.data.event_id
            # 根节点最后处理
            
            elif node.data.call_id != '1' and node.data.call_id != '2' :
                #print '%^&'
                tree.link_past_node(node.data.event_id)
                
                #tree.show()
        # 处理根节点
        #tree.show()
        #print root_event_id
        root_node = tree.get_node(root_event_id)
        root_node.data.parent_event_id = None
        # 删除根节点的话，就把它的子树都存到new_forest里面去
        if root_node.data.call_id != '1' and root_node.data.call_id != '2':
            # 取子节点列表
            children_list = tree.children(root_event_id)
            # 对每个子节点，取以它为根节点的子树,加到new_forest里面去
            for child_ in children_list:
                child_.data.parent_event_id = None
                new_tree = tree.subtree( child_.data.event_id)
                new_forest.append(new_tree)
                #print 'newtree'
                #new_tree.show()
            #print 'new_forest size',len(new_forest)
        # 不删除根节点的话，直接添加这棵树
        else :
            #print 'tree'
            #tree.show()
            new_forest.append(tree)
        #print '***************************'
    # 更新parent_event_id
    for tree in new_forest:
        tree.show()
        #nodelist = tree.all_nodes()
        for node_event_id in tree.expand_tree(mode=Tree.DEPTH, sorting=False):
            node = tree.get_node(node_event_id)
            # print 'node.is_root()',node.is_root()
            print 'node.data.event_id',node.data.event_id
            print 'node.data.parent_event_id',node.data.parent_event_id
            if node.data.parent_event_id is None:
                #print node.data.event_id
                node.data.parent_event_id = None
                
            else:
                # 获得当前的父亲
                parent_node = tree.parent(node.data.event_id)
                node.data.parent_event_id = parent_node.data.parent_event_id
                print "*"
            print 'node.data.event_id',node.data.event_id
            print 'node.data.parent_event_id',node.data.parent_event_id
    return new_forest

def saveResToFiles(fn,forest,TRACEORDER,traceDir,traces):

    TRACEDIR = traceDir + TRACEORDER
    ALLTRACEFILENAME = TRACEDIR+'/' + 'trace'+ TRACEORDER  + '.txt'
    #print TRACEDIR
    mkdir(TRACEDIR)
    list2 = []
    list3 = []
    # 这是一个filter，目前只处理send和recv
    # forest = filterTrace(forest)
    for i in range(len(forest)):
        mkdir(TRACEDIR+'/traces')
        FILENAME = TRACEDIR+'/' + 'traces/trace'+ TRACEORDER + '_' + str(i+1) + '.txt'
        tree = forest[i]
        #tree.show()
        tracelist=tree.all_nodes()
        list1 = []


        for row in tracelist:
            str1 = ('event_id='+str(row.data.event_id)+',ftype_str='+str(row.data.ftype_str)+',on_ip='+str(row.data.on_ip)+',on_port='+str(row.data.on_port)+',in_ip='+str(row.data.in_ip)
        +',in_port='+str(row.data.in_port)+',process_id='+str(row.data.process_id)+',thread_id='+str(row.data.thread_id)+',p_process_id='+str(row.data.p_process_id)+',pname='+str(row.data.pname)
        +',parent_event_id='+str(row.data.parent_event_id)+',time_info='+str(row.data.time_info)+',rtype='+str(row.data.rtype)+',http_response_status_code='+str(row.data.http_response_status_code)+'\n')
            #print(str1)
            list1.append(str1)
            list2.append(str1)
        textSave(FILENAME,list1)
        mkdir(TRACEDIR+'/forest/event_id')
        mkdir(TRACEDIR+'/forest/pname')
        tree.save2file(TRACEDIR+'/forest/event_id/trace'+ TRACEORDER + '_' +str(i+1)+'.txt')
        tree.save2file(filename=TRACEDIR+'/forest/pname/trace'+ TRACEORDER + '_' +str(i+1)+'.txt',data_property="pname")
    textSave(ALLTRACEFILENAME,list2)
    
    
    for row in traces:
        str2 = ('event_id='+str(row.event_id)+',call_id='+str(row.call_id)+',ftype_str='+str(row.ftype_str)+',on_ip='+str(row.on_ip)+',on_port='+str(row.on_port)+',in_ip='+str(row.in_ip)
        +',in_port='+str(row.in_port)+',process_id='+str(row.process_id)+',thread_id='+str(row.thread_id)+',p_process_id='+str(row.p_process_id)+',pname='+str(row.pname)
        +',parent_event_id='+str(row.parent_event_id)+',MSG_ID='+str(row.MSG_ID)+',MSG_CTX_ID='+str(row.MSG_CTX_ID)+',time_info='+str(row.time_info)+',rtype='+str(row.rtype)+',http_response_status_code='+str(row.http_response_status_code)+'\n')
        list3.append(str2)
        # 打印结果
        # print "event_id=%s,f_type=%s,ip=%s,ktid=%s,pid=%s,pname=%s,parent_event_id=%s" % \
                #(event_id, f_type, ip, ktid ,pid, pname, parent_event_id )
    textSave(TRACEDIR+'/' + 'trace'+ TRACEORDER + '_orgin.txt',list3)
    
    newDataName = TRACEDIR+'/trace'+ TRACEORDER +'.data'
    cmd = 'cp '+fn+' '+ newDataName
    os.popen(cmd)

    return forest

if __name__ == '__main__':
    fn = "traceData.dat"
    traceDir = '/home/zhaoyinqin/workload/trace'
    TRACEORDER = '1'
    gen_pname_dict()
    #print pname_dict
    traces=getTraces(fn)
    forest = covTraceToForest(traces)
    saveResToFiles(fn,forest,TRACEORDER,traceDir,traces)
