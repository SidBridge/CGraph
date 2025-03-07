/***************************
@Author: Chunel
@Contact: chunel@foxmail.com
@File: GElementManager.cpp
@Time: 2021/6/2 10:33 下午
@Desc: 
***************************/

#include "GElementManager.h"

CGRAPH_NAMESPACE_BEGIN

GElementManager::GElementManager() = default;

GElementManager::~GElementManager() = default;


CStatus GElementManager::init() {
    CGRAPH_FUNCTION_BEGIN
    status = preRunCheck();
    CGRAPH_FUNCTION_CHECK_STATUS

    status = analyse();
    CGRAPH_FUNCTION_CHECK_STATUS

    for (GElementPtr element : manager_elements_) {
        status = element->fatProcessor(CFunctionType::INIT);
        CGRAPH_FUNCTION_CHECK_STATUS
        element->is_init_ = true;
    }

    CGRAPH_FUNCTION_END
}


CStatus GElementManager::destroy() {
    CGRAPH_FUNCTION_BEGIN

    for (GElementPtr element : manager_elements_) {
        CGRAPH_ASSERT_NOT_NULL(element)

        status = element->fatProcessor(CFunctionType::DESTROY);
        CGRAPH_FUNCTION_CHECK_STATUS
        element->is_init_ = false;
    }

    CGRAPH_FUNCTION_END
}


CStatus GElementManager::run() {
    CGRAPH_NO_SUPPORT
}


CStatus GElementManager::preRunCheck() {
    CGRAPH_FUNCTION_BEGIN

    /** 首先判定，注册的element全部不为空 */
    for (auto element : manager_elements_) {
        CGRAPH_ASSERT_NOT_NULL(element)
    }

    /**
     * 认定图可以连通的判定条件：
     * 1，当前节点仅有一个依赖
     * 2，当前节点依赖的节点，只有一个后继
     * 3，当前节点的依赖的后继，仍是当前节点
     */
    for (GElement* element : manager_elements_) {
        if (1 == element->dependence_.size()
            && 1 == (*element->dependence_.begin())->run_before_.size()
            && (*(element->dependence_.begin()))->run_before_.find(element) != (*(element->dependence_.begin()))->run_before_.end()) {
            element->linkable_ = true;
        }
    }

    CGRAPH_FUNCTION_END
}


CStatus GElementManager::analyse() {
    CGRAPH_FUNCTION_BEGIN

    CSize runElementSize = 0;
    auto totalElementSize = manager_elements_.size();
    para_cluster_arrs_.clear();

    GClusterArr curClusterArr;    // 记录每一层，可以并行的逻辑
    for (GElementPtr element : manager_elements_) {
        if (!element->isRunnable() || element->isLinkable()) {
            continue;
        }

        GCluster curCluster;
        GElementPtr curElement = element;
        curCluster.addElement(curElement);

        /* 将linkable的节点，统一放到一个cluster中 */
        while (1 == curElement->run_before_.size()
               && (*curElement->run_before_.begin())->isLinkable()) {
            // 将下一个放到cluster中处理
            curElement = (*curElement->run_before_.begin());
            curCluster.addElement(curElement);
        }
        curClusterArr.emplace_back(curCluster);
    }
    para_cluster_arrs_.emplace_back(curClusterArr);

    GClusterArr runnableClusterArr;
    while (!curClusterArr.empty() && runElementSize <= totalElementSize) {
        runnableClusterArr = curClusterArr;
        curClusterArr.clear();

        for (GClusterRef cluster : runnableClusterArr) {
            status = cluster.process(true);    // 不执行run方法的process
            CGRAPH_FUNCTION_CHECK_STATUS
        }
        runElementSize += runnableClusterArr.size();

        GElementPtrSet duplications;
        for (GClusterRef cluster : runnableClusterArr) {
            for (GElementPtr element : cluster.cluster_elements_) {
                for (GElementPtr cur : element->run_before_) {
                    /**
                     * 判断element是否需要被加入
                     * 1，该元素是可以执行的
                     * 2，改元素本次循环是第一次被遍历
                     */
                    if (cur->isRunnable() && duplications.end() == duplications.find(cur)) {
                        GCluster curCluster;
                        GElementPtr curElement = cur;
                        curCluster.addElement(curElement);
                        duplications.insert(curElement);

                        while (1 == curElement->run_before_.size()
                               && (*curElement->run_before_.begin())->isLinkable()) {
                            curElement = (*curElement->run_before_.begin());
                            curCluster.addElement(curElement);
                            duplications.insert(curElement);
                        }
                        curClusterArr.emplace_back(curCluster);
                    }
                }
            }
        }

        /* 为空的话，直接退出循环；不为空的话，放入para信息中 */
        if (!curClusterArr.empty()) {
            para_cluster_arrs_.emplace_back(curClusterArr);
        }
    }

    CGRAPH_FUNCTION_END
}


CStatus GElementManager::afterRunCheck(CSize runNodeSize) {
    CGRAPH_FUNCTION_BEGIN

    /* 验证是否所有的内容均被执行过 */
    if (runNodeSize != manager_elements_.size()) {
        CGRAPH_RETURN_ERROR_STATUS("pipeline run element size check failed...")
    }

    /* 需要验证每个cluster里的每个内容是否被执行过一次 */
    for (GClusterArrRef clusterArr : para_cluster_arrs_) {
        for (GClusterRef cluster : clusterArr) {
            if (!cluster.isElementsDone()) {
                CGRAPH_RETURN_ERROR_STATUS("pipeline done status check failed...");
            }
        }
    }

    CGRAPH_FUNCTION_END
}


CStatus GElementManager::add(GElementPtr element) {
    CGRAPH_FUNCTION_BEGIN
    CGRAPH_ASSERT_NOT_NULL(element)

    this->manager_elements_.emplace(element);

    CGRAPH_FUNCTION_END
}


CBool GElementManager::find(GElementPtr element) const {
    if (nullptr == element) {
        return false;
    }

    return manager_elements_.end() != manager_elements_.find(element);
}


CStatus GElementManager::remove(GElementPtr element) {
    CGRAPH_FUNCTION_BEGIN
    CGRAPH_ASSERT_NOT_NULL(element)

    manager_elements_.erase(element);
    CGRAPH_FUNCTION_END
}


CStatus GElementManager::clear() {
    CGRAPH_FUNCTION_BEGIN

    for (auto element : manager_elements_) {
        CGRAPH_DELETE_PTR(element)
    }

    manager_elements_.clear();
    CGRAPH_FUNCTION_END
}

CGRAPH_NAMESPACE_END
